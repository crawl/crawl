/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "message.h"
#include "format.h"

#include <cstdarg>
#include <cstring>
#include <sstream>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "cio.h"
#include "delay.h"
#include "initfile.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "monstuff.h"
#include "notes.h"
#include "religion.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "menu.h"

class message_item {
public:
    msg_channel_type    channel;        // message channel
    int                 param;          // param for channel (god, enchantment)
    unsigned char       colour;
    std::string         text;           // text of message
    int                 repeats;

    message_item() : channel(NUM_MESSAGE_CHANNELS), param(0), colour(BLACK),
                     text(""), repeats(0)
        {
        }
};

// Circular buffer for keeping past messages.
message_item Store_Message[ NUM_STORED_MESSAGES ];    // buffer of old messages
message_item prev_message;
bool did_flush_message = false;

int Next_Message = 0;                                 // end of messages

int Message_Line = 0;                // line of next (previous?) message
int New_Message_Count = 0;

static FILE* _msg_dump_file = NULL;

static bool suppress_messages = false;
static void base_mpr(const char *inf, msg_channel_type channel, int param,
                     unsigned char colour, int repeats = 1,
                     bool check_previous_msg = true);
static unsigned char prepare_message(const std::string& imsg,
                                     msg_channel_type channel,
                                     int param);

namespace msg
{
    std::ostream stream(new mpr_stream_buf(MSGCH_PLAIN));
    std::vector<std::ostream*> stream_ptrs;
    std::vector<mpr_stream_buf*> stream_buffers;

    std::ostream& streams(msg_channel_type chan)
    {
        ASSERT(chan >= 0 &&
               static_cast<unsigned int>(chan) < stream_ptrs.size());
        return *stream_ptrs[chan];
    }

    void initialise_mpr_streams()
    {
        for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        {
            mpr_stream_buf* pmsb =
                new mpr_stream_buf(static_cast<msg_channel_type>(i));
            std::ostream* pos = new std::ostream(pmsb);
            (*pos) << std::nounitbuf;
            stream_ptrs.push_back(pos);
            stream_buffers.push_back(pmsb);
        }
        stream << std::nounitbuf;
    }

    void deinitialise_mpr_streams()
    {
        for (unsigned int i = 0; i < stream_ptrs.size(); ++i)
            delete stream_ptrs[i];
        stream_ptrs.clear();
        for (unsigned int i = 0; i < stream_buffers.size(); ++i)
            delete stream_buffers[i];
        stream_buffers.clear();
    }


    setparam::setparam(int param)
    {
        m_param = param;
    }

    mpr_stream_buf::mpr_stream_buf(msg_channel_type chan) :
        internal_count(0), param(0), muted(false), channel(chan)
    {}

    void mpr_stream_buf::set_param(int p)
    {
        this->param = p;
    }

    void mpr_stream_buf::set_muted(bool m)
    {
        this->muted = m;
    }

    // again, can be improved
    int mpr_stream_buf::overflow(int c)
    {
        if ( muted )
            return 0;

        if ( c == '\n' )
        {
            // null-terminate and print the string
            internal_buf[internal_count] = 0;
            mpr(internal_buf, channel, param);

            internal_count = 0;

            // reset to defaults (param changing isn't sticky)
            set_param(0);
        }
        else
            internal_buf[internal_count++] = c;

        if ( internal_count + 3 > INTERNAL_LENGTH )
        {
            mpr("oops, hit overflow", MSGCH_ERROR);
            internal_count = 0;
            return std::streambuf::traits_type::eof();
        }
        return 0;
    }
}

std::ostream& operator<<(std::ostream& os, const msg::setparam& sp)
{
    msg::mpr_stream_buf* ps = dynamic_cast<msg::mpr_stream_buf*>(os.rdbuf());
    ps->set_param(sp.m_param);
    return os;
}

no_messages::no_messages() : msuppressed(suppress_messages)
{
    suppress_messages = true;
}

no_messages::~no_messages()
{
    suppress_messages = msuppressed;
}

static char god_message_altar_colour( god_type god )
{
    int  rnd;

    switch (god)
    {
    case GOD_SHINING_ONE:
        return (YELLOW);

    case GOD_ZIN:
        return (WHITE);

    case GOD_ELYVILON:
        return (LIGHTBLUE);     // Really, LIGHTGREY but that's plain text.

    case GOD_OKAWARU:
        return (CYAN);

    case GOD_YREDELEMNUL:
        return (coinflip() ? DARKGREY : RED);

    case GOD_BEOGH:
        return (coinflip() ? BROWN : LIGHTRED);

    case GOD_KIKUBAAQUDGHA:
        return (DARKGREY);

    case GOD_XOM:
        return (random2(15) + 1);

    case GOD_VEHUMET:
        rnd = random2(3);
        return ((rnd == 0) ? LIGHTMAGENTA :
                (rnd == 1) ? LIGHTRED
                           : LIGHTBLUE);

    case GOD_MAKHLEB:
        rnd = random2(3);
        return ((rnd == 0) ? RED :
                (rnd == 1) ? LIGHTRED
                           : YELLOW);

    case GOD_TROG:
        return (RED);

    case GOD_NEMELEX_XOBEH:
        return (LIGHTMAGENTA);

    case GOD_SIF_MUNA:
        return (BLUE);

    case GOD_LUGONU:
        return (LIGHTRED);

    case GOD_NO_GOD:
    case NUM_GODS:
    case GOD_RANDOM:
    case GOD_NAMELESS:
        return (YELLOW);
    }
    return (YELLOW);            // for stupid compilers
}

#ifdef USE_COLOUR_MESSAGES

// Returns a colour or MSGCOL_MUTED.
int channel_to_colour( msg_channel_type channel, int param )
{
    if (you.asleep())
        return (DARKGREY);

    char ret;

    switch (Options.channels[ channel ])
    {
    case MSGCOL_PLAIN:
        // Note that if the plain channel is muted, then we're protecting
        // the player from having that spread to other channels here.
        // The intent of plain is to give non-coloured messages, not to
        // suppress them.
        if (Options.channels[ MSGCH_PLAIN ] >= MSGCOL_DEFAULT)
            ret = LIGHTGREY;
        else
            ret = Options.channels[ MSGCH_PLAIN ];
        break;

    case MSGCOL_DEFAULT:
    case MSGCOL_ALTERNATE:
        switch (channel)
        {
        case MSGCH_GOD:
        case MSGCH_PRAY:
            ret = (Options.channels[ channel ] == MSGCOL_DEFAULT)
                   ? god_colour( static_cast<god_type>(param) )
                   : god_message_altar_colour( static_cast<god_type>(param) );
            break;

        case MSGCH_DURATION:
            ret = LIGHTBLUE;
            break;

        case MSGCH_DANGER:
            ret = RED;
            break;

        case MSGCH_WARN:
        case MSGCH_ERROR:
            ret = LIGHTRED;
            break;

        case MSGCH_FOOD:
            if (param) // positive change
                ret = GREEN;
            else
                ret = YELLOW;
            break;

        case MSGCH_INTRINSIC_GAIN:
            ret = GREEN;
            break;

        case MSGCH_RECOVERY:
            ret = LIGHTGREEN;
            break;

        case MSGCH_TALK:
        case MSGCH_TALK_VISUAL:
            ret = WHITE;
            break;

        case MSGCH_MUTATION:
            ret = LIGHTRED;
            break;

        case MSGCH_TUTORIAL:
            ret = MAGENTA;
            break;

        case MSGCH_MONSTER_SPELL:
        case MSGCH_MONSTER_ENCHANT:
        case MSGCH_FRIEND_SPELL:
        case MSGCH_FRIEND_ENCHANT:
            ret = LIGHTMAGENTA;
            break;

        case MSGCH_MONSTER_DAMAGE:
            ret =  ((param == MDAM_DEAD)               ? RED :
                    (param >= MDAM_SEVERELY_DAMAGED)   ? LIGHTRED :
                    (param >= MDAM_MODERATELY_DAMAGED) ? YELLOW
                                                       : LIGHTGREY);
            break;

        case MSGCH_PROMPT:
            ret = CYAN;
            break;

        case MSGCH_DIAGNOSTICS:
        case MSGCH_MULTITURN_ACTION:
            ret = DARKGREY;     // makes it easier to ignore at times -- bwr
            break;

        case MSGCH_PLAIN:
        case MSGCH_FRIEND_ACTION:
        case MSGCH_ROTTEN_MEAT:
        case MSGCH_EQUIPMENT:
        case MSGCH_EXAMINE:
        case MSGCH_EXAMINE_FILTER:
        default:
            ret = param > 0? param : LIGHTGREY;
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
                ret = Options.channels[ channel ];
            else if (Options.channels[ MSGCH_PLAIN ] >= MSGCOL_DEFAULT)
                ret = LIGHTGREY;
            else
                ret = Options.channels[ MSGCH_PLAIN ];
        }
        else
            ret = Options.channels[ channel ];
        break;
    }

    return (ret);
}

#else // don't use colour messages

int channel_to_colour( msg_channel_type channel, int param )
{
    return (LIGHTGREY);
}

#endif

static void do_message_print( msg_channel_type channel, int param,
                              const char *format, va_list argp )
{
    char buff[200];
    vsnprintf( buff, sizeof( buff ), format, argp );
    buff[199] = 0;

    mpr(buff, channel, param);
}

void mprf( msg_channel_type channel, int param, const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( channel, param, format, argp );
    va_end( argp );
}

void mprf( msg_channel_type channel, const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( channel, channel == MSGCH_GOD ? you.religion : 0,
                      format, argp );
    va_end( argp );
}

void mprf( const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( MSGCH_PLAIN, 0, format, argp );
    va_end( argp );
}

static bool _updating_view = false;

void mpr(const char *inf, msg_channel_type channel, int param)
{
    if (_msg_dump_file != NULL)
        fprintf(_msg_dump_file, "%s\n", inf);

    if (crawl_state.game_crashed)
        return;

    if (crawl_state.arena)
    {
        switch(channel)
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

    if (!crawl_state.io_inited)
    {
        if (channel == MSGCH_ERROR)
            fprintf(stderr, "%s\n", inf);
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

    std::string help = inf;
    if (help.find("</") != std::string::npos)
    {
        std::string col = colour_to_str(channel_to_colour(channel));
        if (!col.empty())
            help = "<" + col + ">" + help + "</" + col + ">";

        // Handing over to the experts...
        formatted_mpr(formatted_string::parse_string(help), channel);
        return;
    }

    unsigned char colour = prepare_message(help, channel, param);

    char mbuf[400];
    size_t i = 0;
    const int stepsize  = get_number_of_cols() - 1;
    const size_t msglen = strlen(inf);
    const int lookback_size = (stepsize < 12 ? 0 : 12);

    // If a message is exactly STEPSIZE characters long,
    // it should precisely fit in one line. The printing is thus
    // from I to I + STEPSIZE - 1. Stop when I reaches MSGLEN.
    while (i < msglen || i == 0)
    {
        strncpy( mbuf, inf + i, stepsize );
        mbuf[stepsize] = 0;
        // Did the message break?
        if (i + stepsize < msglen)
        {
            // Yes, find a nicer place to break it.
            int lookback, where = 0;
            for (lookback = 0; lookback < lookback_size; ++lookback)
            {
                where = stepsize - 1 - lookback;
                if (where >= 0 && isspace(mbuf[where]))
                {
                    // aha!
                    break;
                }
            }

            if (lookback != lookback_size)
            {
                // Found a good spot to break.
                mbuf[where] = 0;
                i += where + 1; // Skip past the space!
            }
            else
                i += stepsize;
        }
        else
            i += stepsize;

        base_mpr(mbuf, channel, param, colour);
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
    unsigned width  = get_number_of_cols() - 1;

    for (int i = 0, size = list.size(); i < size; i++)
    {
        std::string new_str = list[i];

        if (size > 0 && i < (size - 2))
            new_str += comma;
        else if (i == (size - 2))
            new_str += andc;
        else if (i == (size - 1))
            new_str += ".";

        if (out.length() + new_str.length() >= width)
        {
            mpr(out.c_str(), channel, param);
            out = new_str;
        }
        else
            out += new_str;
    }
    mpr(out.c_str(), channel, param);
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
            take_note(Note( NOTE_MESSAGE, channel, param, message.c_str() ));
            break;
        }
    }

    if (channel != MSGCH_DIAGNOSTICS && channel != MSGCH_EQUIPMENT
        && channel != MSGCH_TALK && channel != MSGCH_TALK_VISUAL
        && channel != MSGCH_FRIEND_SPELL && channel != MSGCH_FRIEND_ENCHANT
        && channel != MSGCH_FRIEND_ACTION && channel != MSGCH_SOUND)
    {
        interrupt_activity( AI_MESSAGE,
                            channel_to_str(channel) + ":" + message );
    }

    // Any sound has a chance of waking the PC if the PC is asleep.
    if (channel == MSGCH_SOUND)
        you.check_awaken(5);

    // Check messages for all forms of running now.
    if (you.running)
    {
        for (unsigned i = 0; i < Options.travel_stop_message.size(); ++i)
        {
            if (Options.travel_stop_message[i].is_filtered( channel, message ))
            {
                stop_running();
                break;
            }
        }
    }

    if (!Options.sound_mappings.empty())
    {
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

// Adds a given message to the message history.
static void mpr_store_messages(const std::string& message,
                               msg_channel_type channel, int param,
                               unsigned char colour, int repeats = 1)
{
    const int num_lines = crawl_view.msgsz.y;

    bool was_repeat = false;

    if (Options.msg_condense_repeats)
    {
        int prev_message_num = Next_Message - 1;

        if (prev_message_num < 0)
            prev_message_num = NUM_STORED_MESSAGES - 1;

        message_item &prev_msg = Store_Message[prev_message_num];

        if (prev_msg.repeats > 0 && prev_msg.channel == channel
            && prev_msg.param == param && prev_msg.text == message
            && prev_msg.colour == colour)
        {
            prev_msg.repeats += repeats;
            was_repeat = true;
        }
    }

    // Prompt lines are presumably shown to / seen by the player accompanied
    // by a request for input, which should do the equivalent of a more(); to
    // save annoyance, don't bump New_Message_Count for prompts.
    if (channel != MSGCH_PROMPT)
        New_Message_Count++;

    if (Message_Line < num_lines - 1)
        Message_Line++;

    // Reset colour.
    textcolor(LIGHTGREY);

    // Equipment lists just waste space in the message recall.
    if (!was_repeat && channel_message_history(channel))
    {
        // Put the message into Store_Message, and move the '---' line forward
        Store_Message[ Next_Message ].text    = message;
        Store_Message[ Next_Message ].colour  = colour;
        Store_Message[ Next_Message ].channel = channel;
        Store_Message[ Next_Message ].param   = param;
        Store_Message[ Next_Message ].repeats = repeats;
        Next_Message++;

        if (Next_Message >= NUM_STORED_MESSAGES)
            Next_Message = 0;
    }
}

static bool need_prefix = false;

// Does the work common to base_mpr and formatted_mpr.
// Returns the default colour of the message, or MSGCOL_MUTED if
// the message should be suppressed.
static unsigned char prepare_message(const std::string& imsg,
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

    unsigned char colour = (unsigned char) channel_to_colour( channel, param );

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

    if (colour != MSGCOL_MUTED)
        mpr_check_patterns(imsg, channel, param);

    return colour;
}

static void handle_more(int colour)
{
    if (colour != MSGCOL_MUTED)
    {
        flush_input_buffer( FLUSH_ON_MESSAGE );
        const int num_lines = crawl_view.msgsz.y;

        if (New_Message_Count == num_lines - 1)
            more();
    }
}

// Output the previous message.
// Needs to be called whenever the player gets a turn or needs to do
// something, e.g. answer a prompt.
void flush_prev_message()
{
    if (did_flush_message || prev_message.text.empty())
        return;

    did_flush_message = true;
    base_mpr(prev_message.text.c_str(), prev_message.channel,
             prev_message.param, prev_message.colour, prev_message.repeats,
             false);

    prev_message = message_item();
}

static void base_mpr(const char *inf, msg_channel_type channel, int param,
                     unsigned char colour, int repeats, bool check_previous_msg)
{
    if (colour == MSGCOL_MUTED)
        return;

    const std::string imsg = inf;

    if (check_previous_msg)
    {
        if (!prev_message.text.empty())
        {
            // If a message is identical to the previous one, increase the
            // counter.
            if (Options.msg_condense_repeats && prev_message.channel == channel
                && prev_message.param == param && prev_message.text == imsg
                && prev_message.colour == colour)
            {
                prev_message.repeats += repeats;
                return;
            }
            flush_prev_message();
        }

        // Always output prompts right away.
        if (channel == MSGCH_PROMPT)
            prev_message = message_item();
        else
        {
            // Store other messages until later.
            prev_message.text    = imsg;
            prev_message.channel = channel;
            prev_message.param   = param;
            prev_message.colour  = colour;
            prev_message.repeats = repeats;
            did_flush_message = false;
            return;
        }
    }

    handle_more(colour);

    if (need_prefix)
    {
        message_out( Message_Line, colour, "-", 1, false );
        need_prefix = false;
    }

    if (repeats > 1)
    {
        snprintf(info, INFO_SIZE, "%s (x%d)", inf, repeats);
        inf = info;
    }
    message_out( Message_Line, colour, inf,
                 Options.delay_message_clear? 2 : 1 );

    if (channel == MSGCH_PROMPT || channel == MSGCH_ERROR)
        reset_more_autoclear();

    for (unsigned i = 0; i < Options.force_more_message.size(); ++i)
    {
        if (Options.force_more_message[i].is_filtered( channel, imsg ))
        {
            more(true);
            New_Message_Count = 0;
            // One more() is quite enough, thank you!
            break;
        }
    }

    mpr_store_messages(imsg, channel, param, colour, repeats);

    if (channel == MSGCH_ERROR)
        interrupt_activity( AI_FORCE_INTERRUPT );
}


static void mpr_formatted_output(formatted_string fs, int colour)
{
    int curcol = Options.delay_message_clear ? 2 : 1;

    if (need_prefix)
    {
        message_out( Message_Line, colour, "-", 1, false );
        need_prefix = false;
    }

    // Find last text op so that we can scroll the output.
    unsigned last_text = fs.ops.size();
    for (unsigned i = 0; i < fs.ops.size(); ++i)
    {
        if (fs.ops[i].type == FSOP_TEXT)
            last_text = i;
    }

    for (unsigned i = 0; i < fs.ops.size(); ++i)
    {
        switch (fs.ops[i].type)
        {
        case FSOP_COLOUR:
            colour = fs.ops[i].x;
            break;
        case FSOP_TEXT:
            message_out(Message_Line, colour, fs.ops[i].text.c_str(), curcol,
                        (i == last_text));
            curcol += multibyte_strlen(fs.ops[i].text);
            break;
        case FSOP_CURSOR:
            break;
        }
    }
}

// Line wrapping is not available here!
// Note that the colour will be first set to the appropriate channel
// colour before displaying the formatted_string.
void formatted_mpr(const formatted_string& fs, msg_channel_type channel,
                   int param)
{
    flush_prev_message();

    const std::string imsg = fs.tostring();
    const int colour = prepare_message(imsg, channel, param);
    if (colour == MSGCOL_MUTED)
        return;

    handle_more(colour);

    mpr_formatted_output(fs, colour);
    mpr_store_messages(imsg, channel, param, colour);

    if (channel == MSGCH_PROMPT || channel == MSGCH_ERROR)
        reset_more_autoclear();

    if (channel == MSGCH_ERROR)
        interrupt_activity( AI_FORCE_INTERRUPT );
}

// Output given string as formatted message(s), but check patterns
// for string stripped of tags and store the original tagged string
// for message history.  Newlines break the string into multiple
// messages.
//
// If wrap_col > 0, text is wrapped at that column.
//
void formatted_message_history(const std::string &st_nocolor,
                               msg_channel_type channel,
                               int param, int wrap_col)
{
    flush_prev_message();

    if (suppress_messages)
        return;

    int colour = channel_to_colour( channel, param );
    if (colour == MSGCOL_MUTED)
        return;

    // Apply channel color explicitly, so "foo <w>bar</w> baz"
    // renders "baz" correctly
    std::string st;
    {
        std::ostringstream text;
        const std::string colour_str = colour_to_str(colour);
        text << "<" << colour_str << ">"
             << st_nocolor
             << "</" << colour_str << ">";
        text.str().swap(st);
    }

    if (wrap_col)
        linebreak_string2(st, wrap_col);

    std::vector<formatted_string> fss;
    formatted_string::parse_string_to_multiple(st, fss);

    for (unsigned int i = 0; i < fss.size(); i++)
    {
        const formatted_string& fs = fss[i];
        const std::string unformatted = fs.tostring();
        mpr_check_patterns(unformatted, channel, param);

        flush_input_buffer( FLUSH_ON_MESSAGE );

        const int num_lines = crawl_view.msgsz.y;

        if (New_Message_Count == num_lines - 1)
            more();

        mpr_formatted_output(fs, colour);

        for (unsigned f = 0; f < Options.force_more_message.size(); ++f)
        {
            if (Options.force_more_message[f].is_filtered(channel, st_nocolor))
            {
                more(true);
                New_Message_Count = 0;
                // One more() is quite enough, thank you!
                break;
            }
        }
        mpr_store_messages(fs.to_colour_string(), channel, param, colour);
    }
}

bool any_messages(void)
{
    return (Message_Line > 0);
}

void mesclr( bool force )
{
    if (crawl_state.game_crashed)
        return;

    New_Message_Count = 0;

    // If no messages, return.
    if (!any_messages() && !force)
        return;

    if (!force && Options.delay_message_clear)
    {
        need_prefix = true;
        return;
    }

    // Turn cursor off -- avoid 'cursor dance'.

    cursor_control cs(false);
    clear_message_window();
    need_prefix = false;
    Message_Line = 0;
}

static bool autoclear_more = false;

void reset_more_autoclear()
{
    autoclear_more = false;
}

void more(bool user_forced)
{
    if (crawl_state.game_crashed)
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

    if (crawl_state.is_replaying_keys() || autoclear_more)
    {
        mesclr();
        return;
    }

    if (Options.show_more_prompt && !suppress_messages)
    {
        int keypress = 0;

        if (Options.tutorial_left)
        {
            message_out(crawl_view.msgsz.y - 1,
                        LIGHTGREY,
                        "--more--                        "
                        "Press Ctrl-P to reread old messages.",
                        2, false);
        }
        else
        {
            message_out(crawl_view.msgsz.y - 1,
                        LIGHTGREY, "--more--", 2, false);
        }

        mouse_control mc(MOUSE_MODE_MORE);
        do
        {
            keypress = getch();
        }
        while (keypress != ' ' && keypress != '\r' && keypress != '\n'
               && keypress != ESCAPE && keypress != -1
               && (user_forced || keypress != CK_MOUSE_CLICK));

        if (keypress == ESCAPE)
            autoclear_more = true;
    }
    mesclr(true);
}                               // end more()

static bool is_channel_dumpworthy(msg_channel_type channel)
{
    return (channel != MSGCH_EQUIPMENT
            && channel != MSGCH_DIAGNOSTICS
            && channel != MSGCH_TUTORIAL);
}

std::string get_last_messages(int mcount)
{
    if (mcount <= 0)
        return "";

    if (mcount > NUM_STORED_MESSAGES)
        mcount = NUM_STORED_MESSAGES;

    bool full_buffer = (Store_Message[NUM_STORED_MESSAGES - 1].text.empty());
    int initial = Next_Message - mcount;
    if (initial < 0 || initial > NUM_STORED_MESSAGES)
    {
        if (full_buffer)
        {
            initial++;
            initial = (initial + NUM_STORED_MESSAGES) % NUM_STORED_MESSAGES;
        }
        else
            initial = 0;
    }

    std::string text;
    int count = 0;
    for (int i = initial; i != Next_Message; )
    {
        const message_item &msg = Store_Message[i];

        if (!msg.text.empty() && is_channel_dumpworthy(msg.channel))
        {
            text += formatted_string::parse_string(msg.text).tostring();
            text += EOL;
            count++;
        }

        if (++i >= NUM_STORED_MESSAGES)
            i -= NUM_STORED_MESSAGES;
    }

    // An extra line of clearance.
    if (count)
        text += EOL;

    return text;
}

std::vector<std::string> get_recent_messages(int &message_pos,
                                             bool dumpworthy_only,
                                             std::vector<int> *channels)
{
    ASSERT(message_pos >= 0 && message_pos < NUM_STORED_MESSAGES);

    std::vector<int> _channels;
    if (channels == NULL)
        channels = &_channels;

    std::vector<std::string> out;

    while (message_pos != Next_Message)
    {
        const message_item &msg = Store_Message[message_pos++];

        if (message_pos >= NUM_STORED_MESSAGES)
            message_pos -= NUM_STORED_MESSAGES;

        if (msg.text.empty())
            continue;

        if (dumpworthy_only && !is_channel_dumpworthy(msg.channel))
            continue;

        out.push_back(formatted_string::parse_string(msg.text).tostring());
        channels->push_back( (int) msg.channel );
    }

    return (out);
}

void save_messages(writer& outf)
{
    marshallLong( outf, Next_Message );
    for (int i = 0; i < Next_Message; ++i)
    {
         marshallString4( outf, Store_Message[i].text );
         marshallLong( outf, (long) Store_Message[i].channel );
         marshallLong( outf, Store_Message[i].param );
         marshallByte( outf, Store_Message[i].colour );
         marshallLong( outf, Store_Message[i].repeats );
    }
}

void load_messages(reader& inf)
{
    Next_Message = 0;
    int num = unmarshallLong( inf );
    for (int i = 0; i < num; ++i)
    {
        std::string text;
        unmarshallString4( inf, text );

        msg_channel_type channel = (msg_channel_type) unmarshallLong( inf );
        int           param      = unmarshallLong( inf );
        unsigned char colour     = unmarshallByte( inf);
        int           repeats    = unmarshallLong( inf );

        for (int k = 0; k < repeats; k++)
             mpr_store_messages(text, channel, param, colour);
    }
}

void replay_messages(void)
{
    int        win_start_line = 0;
    int        keyin;

    bool       full_buffer = true;
    int        num_msgs = NUM_STORED_MESSAGES;
    int        first_message = Next_Message;

    const int  num_lines = get_number_of_lines();

    if (Store_Message[NUM_STORED_MESSAGES - 1].text.empty())
    {
        full_buffer   = false;
        first_message = 0;
        num_msgs = Next_Message;
    }

    int last_message = Next_Message - 1;
    if (last_message < 0)
        last_message += NUM_STORED_MESSAGES;

    // track back a screen's worth of messages from the end
    win_start_line = Next_Message - (num_lines - 2);
    if (win_start_line < 0)
    {
        if (full_buffer)
            win_start_line += NUM_STORED_MESSAGES;
        else
            win_start_line = 0;
    }

    // Turn off the cursor
    cursor_control cursoff(false);

    while (true)
    {
        clrscr();

        cgotoxy(1, 1, GOTO_CRT);

        for (int i = 0; i < num_lines - 2; i++)
        {
            // Calculate line in circular buffer.
            int line = win_start_line + i;
            if (line >= NUM_STORED_MESSAGES)
                line -= NUM_STORED_MESSAGES;

            // Avoid wrap-around.
            if (line == first_message && i != 0)
                break;

            unsigned char colour = Store_Message[ line ].colour;
            ASSERT(colour != MSGCOL_MUTED);

            textcolor( Store_Message[ line ].colour );

            std::string text = Store_Message[line].text;

            // Allow formatted output of tagged messages.
            formatted_string fs = formatted_string::parse_string(text, true);
            int curcol = 1;
            for (unsigned int j = 0; j < fs.ops.size(); ++j)
            {
                switch ( fs.ops[j].type )
                {
                case FSOP_COLOUR:
                    colour = fs.ops[j].x;
                    break;
                case FSOP_TEXT:
                    textcolor( colour );
                    cgotoxy(curcol, wherey(), GOTO_CRT);
                    cprintf("%s", fs.ops[j].text.c_str());
                    curcol += multibyte_strlen(fs.ops[j].text);
                    break;
                case FSOP_CURSOR:
                    break;
                }
            }
            textcolor(LIGHTGREY);
            if (Store_Message[ line ].repeats > 1)
                cprintf(" (x%d)", Store_Message[ line ].repeats);
            cprintf(EOL);
        }

        // print a footer -- note: relative co-ordinates start at 1
        int rel_start;
        if (!full_buffer)
        {
            if (Next_Message == 0)      // no messages!
                rel_start = 0;
            else
                rel_start = win_start_line + 1;
        }
        else if (win_start_line >= first_message)
            rel_start = win_start_line - first_message + 1;
        else
            rel_start = (win_start_line + NUM_STORED_MESSAGES) - first_message + 1;

        int rel_end = rel_start + (num_lines - 2) - 1;
        if (rel_end > num_msgs)
            rel_end = num_msgs;

        cprintf( "-------------------------------------------------------------------------------" );
        cprintf(EOL);
        cprintf( "<< Lines %d-%d of %d (Up: k<-8, Down: j>+2) >>",
                 rel_start, rel_end, num_msgs );

        keyin = getch();

        if (full_buffer && NUM_STORED_MESSAGES > num_lines - 2
            || !full_buffer && Next_Message > num_lines - 2)
        {
            int new_line;
            int end_mark;

#ifdef USE_TILE
            if (keyin == CK_MOUSE_B4) keyin = '8';
            if (keyin == CK_MOUSE_B5) keyin = '2';
#endif

            if (keyin == 'k' || keyin == '8' || keyin == '-' || keyin == '<')
            {
                new_line = win_start_line - (num_lines - 2);

                // end_mark is equivalent to Next_Message, but
                // is always less than win_start_line.
                end_mark = first_message;
                if (end_mark > win_start_line)
                    end_mark -= NUM_STORED_MESSAGES;

                ASSERT( end_mark <= win_start_line );

                if (new_line <= end_mark)
                    new_line = end_mark;   // hit top

                // handle wrap-around
                if (new_line < 0)
                {
                    if (full_buffer)
                        new_line += NUM_STORED_MESSAGES;
                    else
                        new_line = 0;
                }
            }
            else if (keyin == 'j' || keyin == '2' || keyin == '+'
                     || keyin == '>')
            {
                new_line = win_start_line + (num_lines - 2);

                // As above, but since we're adding we want
                // this mark to always be greater.
                end_mark = last_message;
                if (end_mark < win_start_line)
                    end_mark += NUM_STORED_MESSAGES;

                ASSERT( end_mark >= win_start_line );

                // hit bottom
                if (new_line >= end_mark - (num_lines - 2))
                    new_line = end_mark - (num_lines - 2) + 1;

                if (new_line >= NUM_STORED_MESSAGES)
                    new_line -= NUM_STORED_MESSAGES;
            }
            else
                break;

            win_start_line = new_line;
        }
        else
        {
            if (keyin != 'k' && keyin != '8' && keyin != '-'
                && keyin != 'j' && keyin != '2' && keyin != '+'
                && keyin != '<' && keyin != '>')
            {
                break;
            }
        }
    }

    return;
}                               // end replay_messages()

void set_msg_dump_file(FILE* file)
{
    _msg_dump_file = file;
}
