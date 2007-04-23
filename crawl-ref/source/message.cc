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

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "initfile.h"
#include "libutil.h"
#include "macro.h"
#include "delay.h"
#include "stuff.h"
#include "travel.h"
#include "view.h"
#include "notes.h"
#include "stash.h"
#include "religion.h"
#include "tutorial.h"

// circular buffer for keeping past messages
message_item Store_Message[ NUM_STORED_MESSAGES ];    // buffer of old messages
int Next_Message = 0;                                 // end of messages

int Message_Line = 0;                // line of next (previous?) message
int New_Message_Count = 0;

static bool suppress_messages = false;
static void base_mpr(const char *inf, int channel, int param);

no_messages::no_messages() : msuppressed(suppress_messages)
{
    suppress_messages = true;
}

no_messages::~no_messages()
{
    suppress_messages = msuppressed;
}

static char god_message_altar_colour( char god )
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

    case GOD_NO_GOD:
    default:
        return(YELLOW);
    }
}

#ifdef USE_COLOUR_MESSAGES

// returns a colour or MSGCOL_MUTED
int channel_to_colour( int channel, int param )
{
    char        ret;

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
                                    ? god_colour( param )
                                    : god_message_altar_colour( param );
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
            ret = YELLOW;
            break;

        case MSGCH_INTRINSIC_GAIN:
            ret = GREEN;
            break;

        case MSGCH_RECOVERY:
            ret = LIGHTGREEN;
            break;

        case MSGCH_TALK:
            ret = WHITE;
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

int channel_to_colour( int channel, int param )
{
    return (LIGHTGREY);
}

#endif

static void do_message_print( int channel, int param, 
                              const char *format, va_list argp )
{
    char buff[200];
    vsnprintf( buff, sizeof( buff ), format, argp ); 
    buff[199] = 0;

    mpr(buff, channel, param);
}

void mprf( int channel, int param, const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( channel, param, format, argp );
    va_end( argp );
}    

void mprf( int channel, const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( channel, 0, format, argp );
    va_end( argp );
}

void mprf( const char *format, ... )
{
    va_list  argp;
    va_start( argp, format );
    do_message_print( MSGCH_PLAIN, 0, format, argp );
    va_end( argp );
}

void mpr(const char *inf, int channel, int param)
{
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

// checks whether a given message contains patterns relevant for
// notes, stop_running or sounds and handles these cases
static void mpr_check_patterns(const std::string& message, int channel,
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

    if (channel != MSGCH_DIAGNOSTICS && channel != MSGCH_EQUIPMENT)
        interrupt_activity( AI_MESSAGE, channel_to_str(channel) + ":" + message );

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

// adds a given message to the message history
static void mpr_store_messages(const std::string& message,
                               int channel, int param)
{
    const int num_lines = get_message_window_height();

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
    if (channel != MSGCH_EQUIPMENT)
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
static int prepare_message(const std::string& imsg, int channel, int param)
{
    if (suppress_messages)
        return MSGCOL_MUTED;    

    int colour = channel_to_colour( channel, param );

    const std::vector<message_colour_mapping>& mcm =
        Options.message_colour_mappings;
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
        const int num_lines = get_message_window_height();
    
        if (New_Message_Count == num_lines - 1)
            more();
    }
    return colour;
}

static void base_mpr(const char *inf, int channel, int param)
{
    const std::string imsg = inf;
    const int colour = prepare_message( imsg, channel, param );
    
    if ( colour == MSGCOL_MUTED )
        return;
   
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
            curcol += fs.ops[i].text.length();
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
// XXX This code just reproduces base_mpr(). There must be a better
// way to do this.
void formatted_mpr(const formatted_string& fs, int channel, int param)
{
    const std::string imsg = fs.tostring();
    const int colour = prepare_message(imsg, channel, param);
    if (colour == MSGCOL_MUTED)
        return;

    mpr_formatted_output(fs, colour);
    mpr_store_messages(imsg, channel, param);
}

// output given string as formatted message, but check patterns
// for string stripped of tags and store original tagged string
// for message history
void formatted_message_history(const std::string st, int channel, int param)
{
    if (suppress_messages)
        return;

    int colour = channel_to_colour( channel, param );
    if (colour == MSGCOL_MUTED)
        return;

    formatted_string fs = formatted_string::parse_string(st);

    mpr_check_patterns(fs.tostring(), channel, param);

    flush_input_buffer( FLUSH_ON_MESSAGE );

    const int num_lines = get_message_window_height();

    if (New_Message_Count == num_lines - 1)
        more();

    mpr_formatted_output(fs, colour);

    mpr_store_messages(st, channel, param);
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
    char keypress = 0;

    if (Options.tutorial_left)
        message_out(get_message_window_height() - 1,
                    LIGHTGREY,
                    "--more--                        "
                    "Press Ctrl-P to reread old messages",
                    2, false);
    else
        message_out(get_message_window_height() - 1,
                    LIGHTGREY, "--more--", 2, false);

    do
        keypress = getch();
    while (keypress != ' ' && keypress != '\r' && keypress != '\n');

    mesclr(true);
}                               // end more()

static bool is_channel_dumpworthy(int channel)
{
    return (channel != MSGCH_EQUIPMENT
            && channel != MSGCH_DIAGNOSTICS
            && channel != MSGCH_TUTORIAL);
}

std::string get_last_messages(int mcount)
{
    if (mcount <= 0) return std::string();
    if (mcount > NUM_STORED_MESSAGES) mcount = NUM_STORED_MESSAGES;

    bool full_buffer = Store_Message[ NUM_STORED_MESSAGES - 1 ].text.length() == 0;
    int initial = Next_Message - mcount;
    if (initial < 0 || initial > NUM_STORED_MESSAGES)
        initial = full_buffer? initial + NUM_STORED_MESSAGES : 0;

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
    if (count) text += EOL;

    return text;
}

void replay_messages(void)
{
    int            win_start_line = 0;
    unsigned char  keyin;

    bool           full_buffer = true;
    int            num_msgs = NUM_STORED_MESSAGES;
    int            first_message = Next_Message;

    const int      num_lines = get_number_of_lines();

    if (Store_Message[ NUM_STORED_MESSAGES - 1 ].text.length() == 0)
    {
        full_buffer = false;
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

    for(;;)
    {
        clrscr();

        gotoxy(1, 1);

        for (int i = 0; i < num_lines - 2; i++)
        {
            // calculate line in circular buffer
            int line = win_start_line + i;
            if (line >= NUM_STORED_MESSAGES)
                line -= NUM_STORED_MESSAGES;

            // avoid wrap-around
            if (line == first_message && i != 0)
                break;

            int colour = channel_to_colour( Store_Message[ line ].channel,
                                            Store_Message[ line ].param );
            if (colour == MSGCOL_MUTED)
                continue;

            textcolor( colour );

            std::string text = Store_Message[ line ].text;
            // for tutorial texts (for now, used for debugging)
            // allow formatted output of tagged messages
            if (Store_Message[ line ].channel == MSGCH_TUTORIAL)
            {
                formatted_string fs = formatted_string::parse_string(text);
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
                        gotoxy(curcol, wherey());
                        cprintf(fs.ops[j].text.c_str());
                        curcol += fs.ops[j].text.length();
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
                 
        keyin = get_ch();

        if ((full_buffer && NUM_STORED_MESSAGES > num_lines - 2)
            || (!full_buffer && Next_Message > num_lines - 2))
        {
            int new_line;
            int end_mark;

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
