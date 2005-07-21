/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *      <3>      5/20/99        BWR             Extended screen lines support
 *      <2>      5/08/99        JDJ             mpr takes a const char* instead of a char array.
 *      <1>      -/--/--        LRH             Created
 */

#include "AppHdr.h"
#include "message.h"
#include "religion.h"

#include <string.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "macro.h"
#include "stuff.h"
#include "view.h"


// circular buffer for keeping past messages
message_item Store_Message[ NUM_STORED_MESSAGES ];    // buffer of old messages
int Next_Message = 0;                                 // end of messages

char Message_Line = 0;                // line of next (previous?) message

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
static char channel_to_colour( int channel, int param )
{
    char        ret;

    switch (Options.channels[ channel ])
    {
    case MSGCOL_PLAIN:
        // note that if the plain channel is muted, then we're protecting
        // the player from having that spead to other other channels here.
        // The intent of plain is to give non-coloured messages, not to
        // supress them.
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
            ret = DARKGREY;     // makes is easier to ignore at times -- bwr
            break;

        case MSGCH_PLAIN:
        case MSGCH_ROTTEN_MEAT:
        case MSGCH_EQUIPMENT:
        default:
            ret = LIGHTGREY;
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

static char channel_to_colour( int channel, int param )
{
    return (LIGHTGREY);
}

#endif

void mpr(const char *inf, int channel, int param)
{
    char info2[80];

    int colour = channel_to_colour( channel, param );
    if (colour == MSGCOL_MUTED)
        return;

    you.running = 0;
    flush_input_buffer( FLUSH_ON_MESSAGE );

#ifdef DOS_TERM
    window(1, 1, 80, 25);
#endif

    textcolor(LIGHTGREY);

    const int num_lines = get_number_of_lines();

    if (Message_Line == num_lines - 18) // ( Message_Line == 8 )
        more();

    gotoxy( (Options.delay_message_clear) ? 2 : 1, Message_Line + 18 );
    strncpy(info2, inf, 78);
    info2[78] = 0;

    textcolor( colour );
    cprintf(info2);
    //
    // reset colour
    textcolor(LIGHTGREY);

    Message_Line++;

    if (Options.delay_message_clear 
            && channel != MSGCH_PROMPT 
            && Message_Line == num_lines - 18)
    {
        more();
    }

    // equipment lists just waste space in the message recall
    if (channel != MSGCH_EQUIPMENT)
    {
        /* Put the message into Store_Message, and move the '---' line forward */
        Store_Message[ Next_Message ].text = inf;
        Store_Message[ Next_Message ].channel = channel;
        Store_Message[ Next_Message ].param = param;
        Next_Message++;

        if (Next_Message >= NUM_STORED_MESSAGES)
            Next_Message = 0;
    }
}                               // end mpr()

bool any_messages(void)
{
    return (Message_Line > 0);
}

void mesclr( bool force )
{
    // if no messages, return.
    if (!any_messages())
        return;

    if (!force && Options.delay_message_clear)
    {
        gotoxy( 1, Message_Line + 18 );
        textcolor( channel_to_colour( MSGCH_PLAIN, 0 ) );
        cprintf( ">" );
        return;
    }

    // turn cursor off -- avoid 'cursor dance'
    _setcursortype(_NOCURSOR);

#ifdef DOS_TERM
    window(1, 18, 78, 25);
    clrscr();
    window(1, 1, 80, 25);
#endif

#ifdef PLAIN_TERM
    int startLine = 18;

    gotoxy(1, startLine);

#ifdef LINUX
    clear_to_end_of_screen();
#else

    int numLines = get_number_of_lines() - startLine + 1;
    for (int i = 0; i < numLines; i++)
    {
        cprintf( "                                                                               " );

        if (i < numLines - 1)
        {
            cprintf(EOL);
        }
    }
#endif
#endif

    // turn cursor back on
    _setcursortype(_NORMALCURSOR);

    Message_Line = 0;
}                               // end mseclr()

void more(void)
{
    char keypress = 0;

#ifdef PLAIN_TERM
    gotoxy( 2, get_number_of_lines() );
#endif

#ifdef DOS_TERM
    window(1, 18, 80, 25);
    gotoxy(2, 7);
#endif

    textcolor(LIGHTGREY);

#ifdef DOS
    cprintf(EOL);
#endif
    cprintf("--more--");

    do
    {
        keypress = getch();
    }
    while (keypress != ' ' && keypress != '\r' && keypress != '\n');

    mesclr( (Message_Line >= get_number_of_lines() - 18) );
}                               // end more()

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

#ifdef DOS_TERM
    char buffer[4800];

    window(1, 1, 80, 25);
    gettext(1, 1, 80, 25, buffer);
#endif

    // track back a screen's worth of messages from the end
    win_start_line = Next_Message - (num_lines - 2);
    if (win_start_line < 0)
    {
        if (full_buffer)
            win_start_line += NUM_STORED_MESSAGES;
        else
            win_start_line = 0;
    }

    for(;;)
    {
        // turn cursor off
        _setcursortype(_NOCURSOR);

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

#if DEBUG_DIAGNOSTICS
            cprintf( "%d: %s", line, Store_Message[ line ].text.c_str() );
#else
            cprintf( Store_Message[ line ].text.c_str() );
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
        cprintf( "<< Lines %d-%d of %d >>", rel_start, rel_end, num_msgs );
                 
        // turn cursor back on
        _setcursortype(_NORMALCURSOR);

        keyin = get_ch();

        if ((full_buffer && NUM_STORED_MESSAGES > num_lines - 2)
            || (!full_buffer && Next_Message > num_lines - 2))
        {
            int new_line;
            int end_mark;

            if (keyin == 'k' || keyin == '8' || keyin == '-')
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
            else if (keyin == 'j' || keyin == '2' || keyin == '+')
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
                && keyin != 'j' && keyin != '2' && keyin != '+')
            {
                break;
            }
        }
    }


#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif

    return;
}                               // end replay_messages()
