/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <3>      5/20/99        BWR             Extended screen lines support
 *      <2>      5/08/99        JDJ             mpr takes a const char* instead of a char array.
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
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
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "menu.h"

// circular buffer for keeping past messages
message_item Store_Message[ NUM_STORED_MESSAGES ];    // buffer of old messages
int Next_Message = 0;                                 // end of messages

int Message_Line = 0;                // line of next (previous?) message
int New_Message_Count = 0;

static bool suppress_messages = false;
static void base_mpr(const char *inf, msg_channel_type channel, int param);

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

    void deinitalise_mpr_streams()
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
            mpr("oops, hit overflow", MSGCH_DANGER);
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
        return (LIGHTBLUE);     // really, LIGHTGREY but that's plain text

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
        return (YELLOW);
    }
    return (YELLOW);            // for stupid compilers
}

#ifdef USE_COLOUR_MESSAGES

// returns a colour or MSGCOL_MUTED
int channel_to_colour( msg_channel_type channel, int param )
{
    if (you.asleep())
        return (DARKGREY);

    char ret;

    switch (Options.channels[ channel ])
    {
    case MSGCOL_PLAIN:
        // note that if the plain channel is muted, then we're protecting
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
            ret = LIGHTMAGENTA;
            break;

        case MSGCH_MONSTER_DAMAGE:
            ret =  ((param == MDAM_DEAD)               ? RED :
                    (param >= MDAM_HORRIBLY_DAMAGED)   ? LIGHTRED :
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
            // a special case right now for monster damage (at least until
            // the init system is improved)... selecting a specific
            // colour here will result in only the death messages coloured
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
    do_message_print( channel, channel == MSGCH_GOD? you.religion : 0,
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

void mpr(const char *inf, msg_channel_type channel, int param)
{
    if (!crawl_state.io_inited)
    {
        if (channel == MSGCH_WARN)
            fprintf(stderr, "%s\n", inf);
        return;
    }
    char mbuf[400];
    size_t i = 0;
    const int stepsize = get_number_of_cols() - 1;
    const size_t msglen = strlen(inf);
    const int lookback_size = (stepsize < 12 ? 0 : 12);
    // if a message is exactly STEPSIZE characters long,
    // it should precisely fit in one line. The printing is thus
    // from I to I + STEPSIZE - 1. Stop when I reaches MSGLEN.
    while ( i < msglen || i == 0 )
    {
        strncpy( mbuf, inf + i, stepsize );
        mbuf[stepsize] = 0;
        // did the message break?
        if ( i + stepsize < msglen )
        {
            // yes, find a nicer place to break it.
            int lookback, where = 0;
            for ( lookback = 0; lookback < lookback_size; ++lookback )
            {
                where = stepsize - 1 - lookback;
                if ( where >= 0 && isspace(mbuf[where]) )
                    // aha!
                    break;
            }

            if ( lookback != lookback_size )
            {
                // found a good spot to break
                mbuf[where] = 0;
                i += where + 1; // skip past the space!
            }
            else
                i += stepsize;
        }
        else
            i += stepsize;
        base_mpr( mbuf, channel, param );
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


// checks whether a given message contains patterns relevant for
// notes, stop_running or sounds and handles these cases
static void mpr_check_patterns(const std::string& message,
                               msg_channel_type channel,
                               int param)
{
    for (unsigned i = 0; i < Options.note_messages.size(); ++i)
    {
        if (Options.note_messages[i].matches(message))
        {
            take_note(Note( NOTE_MESSAGE, channel, param, message.c_str() ));
            break;
        }
    }

    if (channel != MSGCH_DIAGNOSTICS && channel != MSGCH_EQUIPMENT
        && channel != MSGCH_TALK && channel != MSGCH_TALK_VISUAL
        && channel != MSGCH_SOUND)
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

// adds a given message to the message history
static void mpr_store_messages(const std::string& message,
                               msg_channel_type channel, int param)
{
    const int num_lines = crawl_view.msgsz.y;

    // Prompt lines are presumably shown to / seen by the player accompanied
    // by a request for input, which should do the equivalent of a more(); to
    // save annoyance, don't bump New_Message_Count for prompts.
    if (channel != MSGCH_PROMPT)
        New_Message_Count++;

    if (Message_Line < num_lines - 1)
        Message_Line++;

    // reset colour
    textcolor(LIGHTGREY);

    // equipment lists just waste space in the message recall
    if (channel_message_history(channel))
    {
        // Put the message into Store_Message, and move the '---' line forward
        Store_Message[ Next_Message ].text = message;
        Store_Message[ Next_Message ].channel = channel;
        Store_Message[ Next_Message ].param = param;
        Next_Message++;

        if (Next_Message >= NUM_STORED_MESSAGES)
            Next_Message = 0;
    }
}

static bool need_prefix = false;

// Does the work common to base_mpr and formatted_mpr.
// Returns the default colour of the message, or MSGCOL_MUTED if
// the message should be suppressed.
static int prepare_message(const std::string& imsg, msg_channel_type channel,
                           int param)
{
    if (suppress_messages)
        return MSGCOL_MUTED;

    int colour = channel_to_colour( channel, param );

    const std::vector<message_colour_mapping>& mcm
               = Options.message_colour_mappings;
    typedef std::vector<message_colour_mapping>::const_iterator mcmci;

    for ( mcmci ci = mcm.begin(); ci != mcm.end(); ++ci )
    {
        if (ci->message.is_filtered(channel, imsg))
        {
            colour = ci->colour;
            break;
        }
    }

    if ( colour != MSGCOL_MUTED )
    {
        mpr_check_patterns(imsg, channel, param);
        flush_input_buffer( FLUSH_ON_MESSAGE );
        const int num_lines = crawl_view.msgsz.y;

        if (New_Message_Count == num_lines - 1)
            more();
    }
    return colour;
}

static void base_mpr(const char *inf, msg_channel_type channel, int param)
{
    const std::string imsg = inf;
    const int colour = prepare_message( imsg, channel, param );

    if ( colour == MSGCOL_MUTED )
        return;

    if (silenced(you.x_pos, you.y_pos) &&
        (channel == MSGCH_SOUND || channel == MSGCH_TALK))
    {
        return;
    }

    if (need_prefix)
    {
        message_out( Message_Line, colour, "-", 1, false );
        need_prefix = false;
    }

    message_out( Message_Line, colour, inf,
                 Options.delay_message_clear? 2 : 1 );

    mpr_store_messages(imsg, channel, param);
}                               // end mpr()


static void mpr_formatted_output(formatted_string fs, int colour)
{
    int curcol = 1;

    if (need_prefix)
    {
        message_out( Message_Line, colour, "-", 1, false );
        ++curcol;
        need_prefix = false;
    }

    for ( unsigned i = 0; i < fs.ops.size(); ++i )
    {
        switch ( fs.ops[i].type )
        {
        case FSOP_COLOUR:
            colour = fs.ops[i].x;
            break;
        case FSOP_TEXT:
            message_out(Message_Line, colour, fs.ops[i].text.c_str(), curcol,
                        false);
            curcol += multibyte_strlen(fs.ops[i].text);
            break;
        case FSOP_CURSOR:
            break;
        }
    }
    message_out( Message_Line, colour, "", Options.delay_message_clear? 2 : 1);
}

// Line wrapping is not available here!
// Note that the colour will be first set to the appropriate channel
// colour before displaying the formatted_string.
void formatted_mpr(const formatted_string& fs, msg_channel_type channel,
                   int param)
{
    const std::string imsg = fs.tostring();
    const int colour = prepare_message(imsg, channel, param);
    if (colour == MSGCOL_MUTED)
        return;

    mpr_formatted_output(fs, colour);
    mpr_store_messages(imsg, channel, param);
}

// output given string as formatted message(s), but check patterns
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
    {
        linebreak_string2(st, wrap_col);
    }

    std::vector<formatted_string> fss;
    formatted_string::parse_string_to_multiple(st, fss);

    for (unsigned int i=0; i<fss.size(); i++)
    {
        const formatted_string& fs = fss[i];
        const std::string unformatted = fs.tostring();
        mpr_check_patterns(unformatted, channel, param);

        flush_input_buffer( FLUSH_ON_MESSAGE );

        const int num_lines = crawl_view.msgsz.y;

        if (New_Message_Count == num_lines - 1)
            more();

        mpr_formatted_output(fs, colour);

        // message playback explicitly only handles colors for
        // the tutorial channel... guess we'll store bare strings
        // for the rest, then.
        if (channel == MSGCH_TUTORIAL)
            mpr_store_messages(fs.to_colour_string(), channel, param);
        else
            mpr_store_messages(unformatted, channel, param);
    }
}

bool any_messages(void)
{
    return (Message_Line > 0);
}

void mesclr( bool force )
{
    New_Message_Count = 0;

    // if no messages, return.
    if (!any_messages())
        return;

    if (!force && Options.delay_message_clear)
    {
        need_prefix = true;
        return;
    }

    // turn cursor off -- avoid 'cursor dance'

    cursor_control cs(false);
    clear_message_window();
    need_prefix = false;
    Message_Line = 0;
}                               // end mseclr()

void more(void)
{
#ifdef DEBUG_DIAGNOSTICS
    if (you.running)
    {
        mesclr();
        return;
    }
#endif

    if (crawl_state.is_replaying_keys())
    {
        mesclr();
        return;
    }

    if (Options.show_more_prompt && !suppress_messages)
    {
        int keypress = 0;

        if (Options.tutorial_left)
            message_out(crawl_view.msgsz.y - 1,
                        LIGHTGREY,
                        "--more--                        "
                        "Press Ctrl-P to reread old messages",
                        2, false);
        else
            message_out(crawl_view.msgsz.y - 1,
                        LIGHTGREY, "--more--", 2, false);

#ifdef USE_TILE
        mouse_control mc(MOUSE_MODE_MORE);
#endif

        do
            keypress = getch();
        while (keypress != ' ' && keypress != '\r' && keypress != '\n'
               && keypress != -1);
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
        return std::string();

    if (mcount > NUM_STORED_MESSAGES)
        mcount = NUM_STORED_MESSAGES;

    bool full_buffer
       = (Store_Message[ NUM_STORED_MESSAGES - 1 ].text.length() == 0);
    int initial = Next_Message - mcount;
    if (initial < 0 || initial > NUM_STORED_MESSAGES)
        initial = full_buffer ? initial + NUM_STORED_MESSAGES : 0;

    std::string text;
    int count = 0;
    for (int i = initial; i != Next_Message; )
    {
        const message_item &msg = Store_Message[i];

        if (msg.text.length() && is_channel_dumpworthy(msg.channel))
        {
            text += msg.text;
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

void replay_messages(void)
{
    int        win_start_line = 0;
    int        keyin;

    bool       full_buffer = true;
    int        num_msgs = NUM_STORED_MESSAGES;
    int        first_message = Next_Message;

    const int  num_lines = get_number_of_lines();

    if (Store_Message[ NUM_STORED_MESSAGES - 1 ].text.length() == 0)
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
            // calculate line in circular buffer
            int line = win_start_line + i;
            if (line >= NUM_STORED_MESSAGES)
                line -= NUM_STORED_MESSAGES;

            // avoid wrap-around
            if (line == first_message && i != 0)
                break;

            int colour = prepare_message( Store_Message[ line ].text,
                                          Store_Message[ line ].channel,
                                          Store_Message[ line ].param );
            if (colour == MSGCOL_MUTED)
                continue;

            textcolor( colour );

            std::string text = Store_Message[ line ].text;
            // for tutorial texts (for now, used for debugging)
            // allow formatted output of tagged messages
            if (Store_Message[ line ].channel == MSGCH_TUTORIAL)
            {
                formatted_string fs = formatted_string::parse_string(text, true);
                int curcol = 1;
                for ( unsigned int j = 0; j < fs.ops.size(); ++j )
                {
                    switch ( fs.ops[j].type )
                    {
                    case FSOP_COLOUR:
                        colour = fs.ops[j].x;
                        break;
                    case FSOP_TEXT:
                        textcolor( colour );
                        cgotoxy(curcol, wherey(), GOTO_CRT);
                        cprintf(fs.ops[j].text.c_str());
                        curcol += multibyte_strlen(fs.ops[j].text);
                        break;
                    case FSOP_CURSOR:
                        break;
                    }
                }
            }
            else
#if DEBUG_DIAGNOSTICS
            cprintf( "%d: %s", line, text.c_str() );
#else
            cprintf( "%s", text.c_str() );
#endif

            cprintf(EOL);
            textcolor(LIGHTGREY);
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

        if ((full_buffer && NUM_STORED_MESSAGES > num_lines - 2)
            || (!full_buffer && Next_Message > num_lines - 2))
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

                // as above, but since we're adding we want
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
