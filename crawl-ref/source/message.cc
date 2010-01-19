/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *
 * Todo:
 *   - --more-- for full message window
 *   - Ctrl-P should start at end of messages
 *   - filtering of messages for various purposes
 *   - Handle resizing properly, in particular initial resize.
 *   - Get rid of print_formatted_paragraph.
 *   - force parameter for mesclr: either get rid of it or
 *     implement it
 *   - Fix things that use mpr when the messagewindow isn't displayed
 *     (yesno() god prompt for example)
 *   - Don't forget to bump save version when merging.
 */

#include "AppHdr.h"

#include "message.h"
#include "format.h"

#include "cio.h"
#include "colour.h"
#include "delay.h"
#include "initfile.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "mon-stuff.h"
#include "notes.h"
#include "options.h"
#include "player.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "tags.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "viewgeom.h"

#include <sstream>

#ifdef WIZARD
#include "luaterp.h"
#endif

struct message_item
{
    msg_channel_type    channel;        // message channel
    int                 param;          // param for channel (god, enchantment)
    std::string         text;           // text of message (tagged string...)
    int                 repeats;
    long                turn;

    message_item() : channel(NUM_MESSAGE_CHANNELS), param(0),
                     text(""), repeats(0), turn(-1)
    {
    }

    message_item(std::string msg, msg_channel_type chan, int par)
        : channel(chan), param(par), text(msg), repeats(1),
          turn(you.num_turns)
    {
    }

    message_item(std::string msg, msg_channel_type chan, int par,
                 int rep, int trn)
        : channel(chan), param(par), text(msg), repeats(rep),
          turn(trn)
    {
    }

    operator bool() const
    {
        return (repeats > 0);
    }

    operator std::string() const
    {
        return ""; // should strip tags from text
    }

    // Tries to condense the argument into this message.
    // Either *this needs to be an empty item, or it must be the
    // same as the argument.
    bool merge(const message_item& other)
    {
        if (! *this)
        {
            *this = other;
            return true;
        }

        if (!Options.msg_condense_repeats)
            return false;
        if (other.channel == channel && other.param == param &&
            other.text == text)
        {
            repeats += other.repeats;
            return true;
        }
        else
            return false;
    }
};

static int _mod(int num, int denom)
{
    ASSERT(denom > 0);
    div_t res = div(num, denom);
    return (res.rem >= 0 ? res.rem : res.rem + denom);
}

template <typename T, int SIZE>
class circ_vec
{
    T data[SIZE];

    int end;   // first unfilled index

    static void inc(int* index)
    {
        ASSERT(*index >= 0 && *index < SIZE);
        *index = _mod(*index + 1, SIZE);
    }

public:
    circ_vec() : end(0) {}

    int size() const
    {
        return SIZE;
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
    }
};

static std::vector<std::string> linebreak(std::string text, int col)
{
    linebreak_string2(text, col);
    std::vector<std::string> ret;
    std::istringstream ss(text);
    std::string item;
    while (std::getline(ss, item))
        ret.push_back(item);
    return ret;
}

class message_window
{
    int next_line;
    int turn_line; // line with the last new-turn dash
    std::vector<formatted_string> lines;

    int height() const
    {
        return crawl_view.msgsz.y;
    }

    int out_height() const
    {
        return (height() - (use_last_line() ? 0 : 1));
    }

    int use_last_line() const
    {
        return (!more_enabled() || use_first_col());
    }

    int out_width() const
    {
        int w = crawl_view.msgsz.x;
        if (use_first_col())
            w--;
        return w;
    }

    void out_line(const formatted_string& line, int n) const
    {
        cgotoxy(1, n + 1, GOTO_MSG);
        line.display();
        cprintf("%*s", out_width() - line.length(), "");
    }

    // Place cursor at end of last non-empty line to handle prompts.
    // TODO: might get rid of this by clearing the whole window when writing,
    //       and then just writing the actual non-empty lines.
    void place_cursor() const
    {
        int i;
        for (i = lines.size() - 1; i >= 0 && lines[i].length() == 0; --i);
        if (i >= 0)
            cgotoxy(lines[i].length() + 1, i + 1, GOTO_MSG);
    }

    void scroll(int n)
    {
        for (int i = 0; i < height() - n; ++i)
            lines[i] = lines[i + n];
        next_line -= n;
        turn_line -= n;
    }

    // Whether to show msgwin-full more prompts.
    bool more_enabled() const
    {
        // TODO: implementation of more() is incomplete for
        //       !Options.clear_messages.
        return (Options.show_more_prompt && Options.clear_messages);
    }

    void make_space(int n)
    {
        int space = height() - next_line;

        // We could use the last line anyway if we know we're
        // showing the last message, but that would require
        // only calling message_window::show when reading
        // input -- that's not currently the case.
        if (!use_last_line())
            space--;

        if (space >= n)
            return;

        if (!more_enabled()
            || !Options.clear_messages && turn_line >= n - space)
        {
            scroll(n - space);
        }
        else
        {
            more();
            clear();
        }
    }

    void more()
    {
        int last_row = crawl_view.msgsz.y;
        cgotoxy(1, last_row, GOTO_MSG);
        textcolor(LIGHTGREY);
        if (use_first_col())
            cprintf("+");
        else
            cprintf("--more--");
        // XXX: getch() calls flush_prev_message -- we
        //      need something raw.
        getch_ck();
    }

    void add_line(const formatted_string& line)
    {
        resize(); // TODO: get rid of this
        lines[next_line] = line;
        next_line++;
    }

public:
    message_window()
        : next_line(0), turn_line(0)
    {
        clear_lines(); // initialize this->lines
    }

    void resize()
    {
        // XXX: broken (why?)
        lines.resize(height());
    }

    void clear_lines()
    {
        lines.clear();
        lines.resize(height());
    }

    bool use_first_col() const
    {
        return (!Options.clear_messages);
    }

    void set_starting_line()
    {
        // TODO: start at end (sometimes?)
        next_line = 0;
    }

    void clear()
    {
        clear_lines();
        set_starting_line();
        show();
    }

    // write to screen (without refresh)
    void show() const
    {
        // XXX: this should not be necessary as formatted_string should
        //      already do it
        textcolor(LIGHTGREY);
        for (size_t i = 0; i < lines.size(); ++i)
            out_line(lines[i], i);
        place_cursor();
        // TODO: maybe output a last line --more--
    }

    // temporary: to be overwritten with next item, e.g. new turn
    //            leading dash or prompt without response
    void add_item(std::string text, char first_col = ' ', bool temporary)
    {
        std::vector<std::string> newlines = linebreak(text, out_width());
        make_space(newlines.size());
        int old = next_line;
        for (size_t i = 0; i < newlines.size(); ++i)
        {
            std::string fc = "";
            if (use_first_col())
            {
                fc = " ";
                if (i == 0)
                    fc[0] = first_col;
            }
            add_line(formatted_string::parse_string(fc + newlines[i]));
        }
        if (temporary)
            next_line = old;
        show();
    }

    void new_turn()
    {
        // Ensure we don't cause a more prompt.
        turn_line = 1;
        add_item(" ", '-', true);
        turn_line = next_line;
    }
};

message_window msgwin;

void display_message_window()
{
    msgwin.show();
}

typedef circ_vec<message_item, NUM_STORED_MESSAGES> store_t;

class message_store
{
    store_t msgs;
    message_item prev_msg;

public:
    void add(const message_item& msg)
    {
        if (msg.channel != MSGCH_PROMPT && prev_msg.merge(msg))
            return;
        flush_prev();
        prev_msg = msg;
        if (msg.channel == MSGCH_PROMPT)
            flush_prev();
    }

    bool have_prev()
    {
        return (prev_msg);
    }

    void store_msg(const message_item& msg)
    {
        bool newturn = (msg.turn > msgs[-1].turn);
        msgs.push_back(msg);
        std::string repeats = "";
        if (msg.repeats > 1)
            repeats = make_stringf(" x%d", msg.repeats);
        msgwin.add_item(msg.text + repeats, newturn ? '-' : ' ', false);
    }

    void flush_prev()
    {
        if (!prev_msg)
            return;
        store_msg(prev_msg);
        prev_msg = message_item();
    }

    // XXX: this should not need to exist
    const store_t& get_store()
    {
        return msgs;
    }
};

// Circular buffer for keeping past messages.
message_store messages;

static FILE* _msg_dump_file = NULL;

static bool suppress_messages = false;
static msg_colour_type prepare_message(const std::string& imsg,
                                       msg_channel_type channel,
                                       int param);

no_messages::no_messages() : msuppressed(suppress_messages)
{
    suppress_messages = true;
}

no_messages::~no_messages()
{
    suppress_messages = msuppressed;
}

msg_colour_type msg_colour(int col)
{
    return static_cast<msg_colour_type>(col);
}

int colour_msg(msg_colour_type col)
{
    return static_cast<int>(col);
}

#ifdef USE_COLOUR_MESSAGES

// Returns a colour or MSGCOL_MUTED.
msg_colour_type channel_to_colour(msg_channel_type channel, int param)
{
    if (you.asleep())
        return (MSGCOL_DARKGREY);

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
        case MSGCH_PRAY:
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

        case MSGCH_FOOD:
            if (param) // positive change
                ret = MSGCOL_GREEN;
            else
                ret = MSGCOL_YELLOW;
            break;

        case MSGCH_INTRINSIC_GAIN:
            ret = MSGCOL_GREEN;
            break;

        case MSGCH_RECOVERY:
            ret = MSGCOL_LIGHTGREEN;
            break;

        case MSGCH_TALK:
        case MSGCH_TALK_VISUAL:
            ret = MSGCOL_WHITE;
            break;

        case MSGCH_MUTATION:
            ret = MSGCOL_LIGHTRED;
            break;

        case MSGCH_TUTORIAL:
            ret = MSGCOL_MAGENTA;
            break;

        case MSGCH_MONSTER_SPELL:
        case MSGCH_MONSTER_ENCHANT:
        case MSGCH_FRIEND_SPELL:
        case MSGCH_FRIEND_ENCHANT:
            ret = MSGCOL_LIGHTMAGENTA;
            break;

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

    return (ret);
}

#else // don't use colour messages

msg_colour_type channel_to_colour(msg_channel_type channel, int param)
{
    return (MSGCOL_LIGHTGREY);
}

#endif

static void do_message_print(msg_channel_type channel, int param,
                             const char *format, va_list argp)
{
    char buff[200];
    size_t len = vsnprintf( buff, sizeof( buff ), format, argp );
    if (len < sizeof( buff )) {
        mpr( buff, channel, param );
    }
    else {
        char *heapbuf = (char*)malloc( len + 1 );
        vsnprintf( heapbuf, len + 1, format, argp );
        mpr( heapbuf, channel, param );
        free( heapbuf );
    }
}

void mprf(msg_channel_type channel, int param, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, param, format, argp);
    va_end(argp);
}

void mprf(msg_channel_type channel, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, channel == MSGCH_GOD ? you.religion : 0,
                     format, argp);
    va_end(argp);
}

void mprf(const char *format, ...)
{
    va_list  argp;
    va_start(argp, format);
    do_message_print(MSGCH_PLAIN, 0, format, argp);
    va_end(argp);
}

#ifdef DEBUG_DIAGNOSTICS
void dprf( const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( MSGCH_DIAGNOSTICS, 0, format, argp );
    va_end( argp );
}
#endif

static bool _updating_view = false;

static bool check_more(std::string line, msg_channel_type channel)
{
    for (unsigned i = 0; i < Options.force_more_message.size(); ++i)
        if (Options.force_more_message[i].is_filtered(channel, line))
            return true;
    return false;
}

static void debug_channel_arena(msg_channel_type channel)
{
    switch (channel)
    {
    case MSGCH_PROMPT:
    case MSGCH_GOD:
    case MSGCH_PRAY:
    case MSGCH_DURATION:
    case MSGCH_FOOD:
    case MSGCH_RECOVERY:
    case MSGCH_INTRINSIC_GAIN:
    case MSGCH_MUTATION:
    case MSGCH_ROTTEN_MEAT:
    case MSGCH_EQUIPMENT:
    case MSGCH_FLOOR_ITEMS:
    case MSGCH_MULTITURN_ACTION:
    case MSGCH_EXAMINE:
    case MSGCH_EXAMINE_FILTER:
    case MSGCH_TUTORIAL:
        DEBUGSTR("Invalid channel '%s' in arena mode",
                 channel_to_str(channel).c_str());
        break;
    default:
        break;
    }
}

void mpr(std::string text, msg_channel_type channel, int param)
{
    if (_msg_dump_file != NULL)
        fprintf(_msg_dump_file, "%s\n", text.c_str());

    if (crawl_state.game_crashed)
        return;

    if (crawl_state.arena)
        debug_channel_arena(channel);

    if (!crawl_state.io_inited)
    {
        if (channel == MSGCH_ERROR)
            fprintf(stderr, "%s\n", text.c_str());
        return;
    }

    // Flush out any "comes into view" monster announcements before the
    // monster has a chance to give any other messages.
    if (!_updating_view)
    {
        _updating_view = true;
        flush_comes_into_view();
        _updating_view = false;
    }

    if (channel == MSGCH_GOD && param == 0)
        param = you.religion;

    msg_colour_type colour = prepare_message(text, channel, param);

    if (colour == MSGCOL_MUTED)
        return;

    std::string col = colour_to_str(colour_msg(colour));
    text = "<" + col + ">" + text + "</" + col + ">"; // XXX
    messages.add(message_item(text, channel, param));

    if (channel == MSGCH_ERROR)
        interrupt_activity(AI_FORCE_INTERRUPT);
}

int msgwin_get_line(std::string prompt, char *buf, int len,
                    input_history *mh, int (*keyproc)(int& c))
{
    // XXX: cancelable_get_line also flushes, which messes things
    //      up if we haven't flushed already
    flush_prev_message();
    msg_colour_type colour = prepare_message(prompt, MSGCH_PROMPT, 0);
    std::string col = colour_to_str(colour_msg(colour));
    std::string text = "<" + col + ">" + prompt + "</" + col + ">"; // XXX
    msgwin.add_item(text, ' ', true);
    msgwin.show();
    int ret = cancelable_get_line(buf, len, mh, keyproc);
    if (ret >= 0)
        text += buf;
    messages.add(message_item(text, MSGCH_PROMPT, 0));
    return ret;
}

void msgwin_new_turn()
{
    flush_prev_message();
    msgwin.new_turn();
}

// mpr() an arbitrarily long list of strings without truncation or risk
// of overflow.
void mpr_comma_separated_list(const std::string prefix,
                              const std::vector<std::string> list,
                              const std::string &andc,
                              const std::string &comma,
                              const msg_channel_type channel,
                              const int param)
{
    std::string out = prefix;

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
    mpr(out, channel, param);
}


// Checks whether a given message contains patterns relevant for
// notes, stop_running or sounds and handles these cases.
static void mpr_check_patterns(const std::string& message,
                               msg_channel_type channel,
                               int param)
{
    for (unsigned i = 0; i < Options.note_messages.size(); ++i)
    {
        if (channel == MSGCH_EQUIPMENT || channel == MSGCH_FLOOR_ITEMS
            || channel == MSGCH_MULTITURN_ACTION
            || channel == MSGCH_EXAMINE || channel == MSGCH_EXAMINE_FILTER
            || channel == MSGCH_TUTORIAL)
        {
            continue;
        }

        if (Options.note_messages[i].matches(message))
        {
            take_note(Note(NOTE_MESSAGE, channel, param, message.c_str()));
            break;
        }
    }

    if (channel != MSGCH_DIAGNOSTICS && channel != MSGCH_EQUIPMENT
        && channel != MSGCH_TALK && channel != MSGCH_TALK_VISUAL
        && channel != MSGCH_FRIEND_SPELL && channel != MSGCH_FRIEND_ENCHANT
        && channel != MSGCH_FRIEND_ACTION && channel != MSGCH_SOUND)
    {
        interrupt_activity(AI_MESSAGE,
                           channel_to_str(channel) + ":" + message);
    }

    // Any sound has a chance of waking the PC if the PC is asleep.
    if (channel == MSGCH_SOUND)
        you.check_awaken(5);

    // Check messages for all forms of running now.
    if (you.running)
        for (unsigned i = 0; i < Options.travel_stop_message.size(); ++i)
            if (Options.travel_stop_message[i].is_filtered( channel, message ))
            {
                stop_running();
                break;
            }

    if (!Options.sound_mappings.empty())
        for (unsigned i = 0; i < Options.sound_mappings.size(); i++)
        {
            // Maybe we should allow message channel matching as for
            // travel_stop_message?
            if (Options.sound_mappings[i].pattern.matches(message))
            {
                play_sound(Options.sound_mappings[i].soundfile.c_str());
                break;
            }
        }
}

static bool channel_message_history(msg_channel_type channel)
{
    switch (channel)
    {
    case MSGCH_PROMPT:
    case MSGCH_EQUIPMENT:
    case MSGCH_EXAMINE_FILTER:
       return (false);
    default:
       return (true);
    }
}

// Returns the default colour of the message, or MSGCOL_MUTED if
// the message should be suppressed.
static msg_colour_type prepare_message(const std::string& imsg,
                                       msg_channel_type channel,
                                       int param)
{
    if (suppress_messages)
        return MSGCOL_MUTED;

    if (silenced(you.pos())
        && (channel == MSGCH_SOUND || channel == MSGCH_TALK))
    {
        return MSGCOL_MUTED;
    }

    msg_colour_type colour = channel_to_colour(channel, param);

    if (colour != MSGCOL_MUTED)
        mpr_check_patterns(imsg, channel, param);

    const std::vector<message_colour_mapping>& mcm
               = Options.message_colour_mappings;
    typedef std::vector<message_colour_mapping>::const_iterator mcmci;

    for (mcmci ci = mcm.begin(); ci != mcm.end(); ++ci)
    {
        if (ci->message.is_filtered(channel, imsg))
        {
            colour = ci->colour;
            break;
        }
    }

    return colour;
}

void flush_prev_message()
{
    messages.flush_prev();
}

void mesclr(bool force)
{
    // Unflushed message will be lost with clear_messages,
    // so they shouldn't really exist, but some of the delay
    // code appears to do this intentionally.
    // ASSERT(!messages.have_prev());
    flush_prev_message();

    if (Options.clear_messages)
        msgwin.clear();

    // TODO: we could indicate indicate mesclr with a different
    //       leading character than '-'.
}

void more(bool user_forced)
{
    if (crawl_state.game_crashed || crawl_state.seen_hups)
        return;

#ifdef DEBUG_DIAGNOSTICS
    if (you.running)
    {
        mesclr();
        return;
    }
#endif

    if (crawl_state.arena)
    {
        delay(Options.arena_delay);
        mesclr();
        return;
    }

    if (crawl_state.is_replaying_keys())
    {
        mesclr();
        return;
    }

#ifdef WIZARD
    if(luaterp_running())
    {
        mesclr();
        return;
    }
#endif

    if (Options.show_more_prompt && !suppress_messages)
    {
        mpr("--more--", MSGCH_PROMPT);
        getch();
    }

    mesclr(true);
}

static bool is_channel_dumpworthy(msg_channel_type channel)
{
    return (channel != MSGCH_EQUIPMENT
            && channel != MSGCH_DIAGNOSTICS
            && channel != MSGCH_TUTORIAL);
}

std::string get_last_messages(int mcount)
{
    std::string text = EOL;
    message_item msg;
    // XXX: should be using iterators here
    for (int i = 0; i < mcount && (msg = messages.get_store()[-i-1]); ++i)
    {
        if (is_channel_dumpworthy(msg.channel))
        {
            text = formatted_string::parse_string(msg.text).tostring()
                 + EOL + text;
        }
    }

    // An extra line of clearance.
    if (!text.empty())
        text += EOL;
    return text;
}

// We just write out the whole message store including empty/unused
// messages. They'll be ignored when restoring.
void save_messages(writer& outf)
{
    store_t msgs = messages.get_store();
    marshallLong(outf, msgs.size());
    for (int i = 0; i < msgs.size(); ++i)
    {
        marshallString4(outf, msgs[i].text);
        marshallLong(outf, msgs[i].channel);
        marshallLong(outf, msgs[i].param);
        marshallLong(outf, msgs[i].repeats);
        marshallLong(outf, msgs[i].turn);
    }
}

void load_messages(reader& inf)
{
    unwind_var<bool> save_more(Options.show_more_prompt, false);

    int num = unmarshallLong(inf);
    for (int i = 0; i < num; ++i)
    {
        std::string text;
        unmarshallString4(inf, text);

        msg_channel_type channel = (msg_channel_type) unmarshallLong(inf);
        int           param      = unmarshallLong(inf);
        int           repeats    = unmarshallLong(inf);
        int           turn       = unmarshallLong(inf);

        message_item msg(message_item(text, channel, param, repeats, turn));
        if (msg)
            messages.store_msg(msg);
    }
    // With Options.message_clear, we don't want the message window
    // pre-filled.
    mesclr();
}

void replay_messages(void)
{
    formatted_scroller hist;
    const store_t msgs = messages.get_store();
    for (int i = 0; i < msgs.size(); ++i)
        hist.add_text(msgs[i].text);
    hist.show();    
}

void set_msg_dump_file(FILE* file)
{
    _msg_dump_file = file;
}


void formatted_mpr(const formatted_string& fs,
                   msg_channel_type channel, int param)
{
    mpr(fs.to_colour_string(), channel, param);
}

