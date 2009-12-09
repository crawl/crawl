/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "message.h"
#include "format.h"

#include <cstdarg>
#include <cstring>
#include <sstream>



#include "cio.h"
#include "colour.h"
#include "delay.h"
#include "initfile.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-stuff.h"
#include "notes.h"
#include "options.h"
#include "random.h"
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
#include "menu.h"

#ifdef WIZARD
#include "luaterp.h"
#endif

struct message_item
{
    msg_channel_type    channel;        // message channel
    int                 param;          // param for channel (god, enchantment)
    std::string         text;           // text of message (tagged string...)
    int                 repeats;
    bool                cleared_after;  // XXX: this may not stay

    message_item() : channel(NUM_MESSAGE_CHANNELS), param(0),
                     text(""), repeats(0), cleared_after(false)
    {
    }

    message_item(std::string msg, msg_channel_type chan, int par)
        : channel(chan), param(par), text(msg), repeats(1),
          cleared_after(false)
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

    int start; // first filled index
    int end;   // first unfilled index

    static void inc(int* index)
    {
        ASSERT(*index >= 0 && *index < SIZE);
        *index = _mod(*index + 1, SIZE);
    }

    bool full()
    {
        // Wasting one slot so we don't need more
        // than "start" and "end".
        return (size() >= SIZE - 1);
    }

public:
    circ_vec() : start(0), end(0) {}

    int size()
    {
        return _mod(end - start, SIZE);
    }

    T& operator[](int i)
    {
        ASSERT(_mod(i, SIZE) < size());
        return data[_mod(start + i, SIZE)];
    }

    const T& operator[](int i) const
    {
        ASSERT(_mod(i, SIZE) < size());
        return data[_mod(start + i, SIZE)];
    }

    void push_back(const T& item)
    {
        if (full())
            inc(&start);
        data[end] = item;
        inc(&end);
    }
};

class message_store
{
    circ_vec<message_item, NUM_STORED_MESSAGES> msgs;
    message_item prev_msg;

public:
    bool add(const message_item& msg)
    {
        if (prev_msg.merge(msg))
            return false;
        msgs.push_back(prev_msg);
        prev_msg = msg;
        return false; // need_more
    }

    bool have_prev()
    {
        return (prev_msg);
    }

    void flush_prev()
    {
        if (!prev_msg)
            return;
        msgs.push_back(prev_msg);
        prev_msg = message_item();
    }

    void clear_after_last()
    {
        msgs[-1].cleared_after = true;
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

msg_colour_type msg_colour(char col)
{
    return static_cast<msg_colour_type>(col);
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
    vsnprintf( buff, sizeof( buff ), format, argp );
    buff[199] = 0;

    mpr(buff, channel, param);
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

    linebreak_string2(text, 80); // XXX: magic number
    text = text + "\n";
    // TODO: wrap text in <colour>.

    std::vector<std::string> lines = split_string("\n", text, false);
    for (unsigned int i = 0; i < lines.size(); ++i)
    {
        bool need_more = messages.add(message_item(lines[i], channel, param));
        bool force_more = check_more(lines[i], channel);
        if (need_more || force_more)
            more(force_more);
    }
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

// TODO: * make any refresh call flush_prev_message
//       * actually write out to the message window
//           delay: just write out last n messages
//           nodelay: write out since last mesclr
//       * "more" handling

/* TODO
    if (channel == MSGCH_ERROR)
        interrupt_activity(AI_FORCE_INTERRUPT);
*/

void mesclr(bool force)
{
    //ASSERT(!messages.have_prev());
    //messages.clear_after_last();
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
        flush_prev_message();
        // XXX: show more prompt here and wait for key
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
/*
    for (message_store::reverse_iterator msg = message_history.rbegin();
         msg != message_history.rend() && mcount > 0; ++msg)
    {
        if (!msg->text.empty() && is_channel_dumpworth(msg->channel))
        {
            text = formatted_string::parse_string(msg->text).tostring()
                 + EOL + text;
            mcount--;
        }
    }

    // An extra line of clearance.
    if (!text.empty())
        text += EOL;
*/
    return text;
}

void save_messages(writer& outf)
{
    marshallLong(outf, 0);
}

void load_messages(reader& inf)
{
    int num = unmarshallLong(inf);
    for (int i = 0; i < num; ++i)
    {
        std::string text;
        unmarshallString4( inf, text );

        msg_channel_type channel = (msg_channel_type) unmarshallLong(inf);
        int           param      = unmarshallLong(inf);
        unsigned char colour     = unmarshallByte(inf);
        int           repeats    = unmarshallLong(inf);
    }
}

void replay_messages(void)
{
    // TODO: reimplement using formatted_scroller
}

void set_msg_dump_file(FILE* file)
{
    _msg_dump_file = file;
}
