/**
 * @file
 * @brief Functions used to print messages.
**/

#pragma once

#include <iostream>
#include <streambuf>
#include <string>
#include <sstream>
#include <vector>

#include "mpr.h"
#include "canned-message-type.h"
#include "enum.h"
#include "player.h"

using std::vector;

// Write the message window contents out.
void display_message_window();
void clear_message_window();

void scroll_message_window(int n);

void clear_messages(bool force = false);

void flush_prev_message();

void more(bool user_forced = false);

void canned_msg(canned_message_type which_message);

bool simple_monster_message(const monster& mons, const char *event,
                            bool need_possessive = false,
                            msg_channel_type channel = MSGCH_PLAIN,
                            int param = 0,
                            description_level_type descrip = DESC_THE);

string god_speaker(god_type which_deity = you.religion);
void simple_god_message(const char *event, bool need_possessive = false,
                        god_type which_deity = you.religion);
void wu_jian_sifu_message(const char *event);

class formatted_string;

void formatted_mpr(const formatted_string& fs,
                   msg_channel_type channel = MSGCH_PLAIN, int param = 0);

// mpr() an arbitrarily long list of strings
void mpr_comma_separated_list(const string &prefix,
                              const vector<string> &list,
                              const string &andc = ", and ",
                              const string &comma = ", ",
                              const msg_channel_type channel = MSGCH_PLAIN,
                              const int param = 0);

#include "cio.h"

// Sets whether messages that are printed through mpr are
// considered temporary.
void msgwin_set_temporary(bool temp);
// Clear the last set of temporary messages from both
// message window and history.
class msgwin_temporary_mode
{
public:
    msgwin_temporary_mode();
    ~msgwin_temporary_mode();
private:
    bool previous;
};

void msgwin_clear_temporary();

void msgwin_prompt(string prompt);
void msgwin_reply(string reply);

unsigned int msgwin_lines();
unsigned int msgwin_line_length();

// Tell the message window that previous messages may be considered
// read, e.g. after reading input from the player.
void msgwin_got_input();

int msgwin_get_line(string prompt,
                    char *buf, int len,
                    input_history *mh = nullptr,
                    const string &fill = "");

// Do not use this templated function directly. Use the macro below instead.
template<int> static int msgwin_get_line_autohist_temp(string prompt,
                                                       char *buf, int len,
                                                       const string &fill = "")
{
    static input_history hist(10);
    return msgwin_get_line(prompt, buf, len, &hist, fill);
}

// This version of mswgin_get_line will automatically retain its own
// input history, independent of other calls to msgwin_get_line.
#define msgwin_get_line_autohist(...) \
    msgwin_get_line_autohist_temp<__LINE__>(__VA_ARGS__)

// Tell the message window that the game is about to read a new
// command from the player.
void msgwin_new_cmd();
// Tell the message window that a new turn has started.
void msgwin_new_turn();

namespace msg
{
    bool uses_stderr(msg_channel_type channel);

    class tee
    {
    public:
        tee();
        tee(string &_target);
        void force_update();
        virtual ~tee();
        virtual void append(const string &s, msg_channel_type ch = MSGCH_PLAIN);
        virtual void append_line(const string &s, msg_channel_type ch = MSGCH_PLAIN);
        virtual string get_store() const;

    private:
        stringstream store;
        string *target;
    };

    class force_stderr
    {
    public:
        force_stderr(maybe_bool f);
        ~force_stderr();
    private:
        maybe_bool prev_state;
    };

    class suppress
    {
    public:
        suppress();
        suppress(bool really_suppress);
        suppress(msg_channel_type _channel);
        ~suppress();
    private:
        bool msuppressed;
        msg_channel_type channel;
        msg_colour_type prev_colour;
    };
}

void webtiles_send_messages(); // does nothing unless USE_TILE_WEB is defined
void webtiles_send_more_text(string);

void save_messages(writer& outf);
void load_messages(reader& inf);
void clear_message_store();

// Have any messages been printed since the last clear?
bool any_messages();

void replay_messages();
void replay_messages_during_startup();

void set_more_autoclear(bool on);

string get_last_messages(int mcount, bool full = false);
bool recent_error_messages();

int channel_to_colour(msg_channel_type channel, int param = 0);
bool strip_channel_prefix(string &text, msg_channel_type &channel,
                          bool silence = false);

namespace msg
{
    extern ostream stream;
    ostream& streams(msg_channel_type chan = MSGCH_PLAIN);

    struct setparam
    {
        setparam(int param);
        int m_param;
    };

    struct capitalisation
    {
        capitalisation(bool cap);
        bool m_cap;
    };
    extern capitalisation cap, nocap;

    struct mute
    {
        mute(bool value = true);
        bool m_value;
    };

    class mpr_stream_buf : public streambuf
    {
    public:
        mpr_stream_buf(msg_channel_type chan);
        virtual ~mpr_stream_buf() {}
        void set_param(int p);
        void set_muted(bool m);
        void set_capitalise(bool m);
    protected:
        int overflow(int c);
    private:
        static const int INTERNAL_LENGTH = 500;
        char internal_buf[500]; // if your terminal is wider than this, too bad
        int internal_count;
        int param;
        bool muted;
        bool capitalise;
        msg_channel_type channel;
    };

    void initialise_mpr_streams();
    void deinitialise_mpr_streams();
}

ostream& operator<<(ostream& os, const msg::setparam& sp);
ostream& operator<<(ostream& os, const msg::capitalisation& cap);

void set_msg_dump_file(FILE* file);
