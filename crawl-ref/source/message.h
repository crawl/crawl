/*
 *  File:       message.cc
 *  Summary:    Functions used to print messages.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <2>     5/08/99        JDJ             mpr takes a const char* instead of a char array.
 *               <1>     -/--/--        LRH             Created
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <streambuf>
#include <iostream>

#include "externs.h"

struct message_item {
    msg_channel_type    channel;        // message channel
    int                 param;          // param for channel (god, enchantment)
    std::string         text;           // text of message
};


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - command - direct - effects - item_use -
 *              misc - player - spell - spl-book - spells1 - spells2 -
 *              spells3
 * *********************************************************************** */
void mesclr( bool force = false );


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr - bang - beam - decks - fight - files - it_use3 -
 *              item_use - items - message - misc - ouch - player -
 *              religion - spell - spells - spells2 - spells3
 * *********************************************************************** */
void more(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: ability - acr - bang - beam - chardump - command - debug -
 *              decks - direct - effects - fight - files - food - it_use2 -
 *              it_use3 - item_use - items - macro - misc - monplace -
 *              monstuff - mstuff2 - mutation - ouch - overmap - player -
 *              religion - shopping - skills - spell - spl-book - spells -
 *              spells1 - spells2 - spells3 - spells4 - stuff - transfor -
 *              view
 * *********************************************************************** */
void mpr(const char *inf, msg_channel_type channel = MSGCH_PLAIN, int param=0);

class formatted_string;

void formatted_mpr(const formatted_string& fs,
                   msg_channel_type channel = MSGCH_PLAIN, int param = 0);
                   
void formatted_message_history(const std::string &st,
                               msg_channel_type channel = MSGCH_PLAIN,
                               int param = 0);
                               
// 4.1-style mpr, currently named mprf for minimal disruption.
void mprf( msg_channel_type channel, int param, const char *format, ... );
void mprf( msg_channel_type channel, const char *format, ... );
void mprf( const char *format, ... );

class no_messages
{
public:
    no_messages();
    ~no_messages();
private:
    bool msuppressed;
};

bool any_messages();
void replay_messages();

void set_colour(char set_message_colour);


// last updated 13oct2003 {dlb}
/* ***********************************************************************
 * called from: chardump
 * *********************************************************************** */
std::string get_last_messages(int mcount);

int channel_to_colour( msg_channel_type channel, int param = 0 );

namespace msg
{
    extern std::ostream stream;
    std::ostream& streams(msg_channel_type chan = MSGCH_PLAIN);

    struct setparam
    {
        setparam(int param);
        int m_param;
    };

    struct mute
    {
        mute(bool value = true);
        bool m_value;
    };

    class mpr_stream_buf : public std::streambuf
    {
    public:
        mpr_stream_buf(msg_channel_type chan);
        void set_param(int p);
        void set_muted(bool m);
    protected:
        int overflow(int c);
    private:
        static const int INTERNAL_LENGTH = 500;
        char internal_buf[500]; // if your terminal is wider than this, too bad
        int internal_count;
        int param;
        bool muted;
        msg_channel_type channel;
    };

    void initialise_mpr_streams();
    void deinitalise_mpr_streams();
}

std::ostream& operator<<(std::ostream& os, const msg::setparam& sp);


#endif
