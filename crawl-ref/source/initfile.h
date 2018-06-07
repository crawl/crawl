/**
 * @file
 * @brief Simple reading of init file.
**/

#pragma once

#include <cstdio>
#include <string>

#include "enum.h"
#include "game-type.h"
#include "item-prop-enum.h"
#include "job-type.h"
#include "unicode.h"

int str_to_summon_type(const string &str);
string gametype_to_str(game_type type);

job_type str_to_job(const string &str);

string find_crawlrc();
void read_init_file(bool runscript = false);

struct newgame_def;
newgame_def read_startup_prefs();

void read_options(const string &s, bool runscript = false,
                  bool clear_aliases = false);

void get_system_environment();

class depth_ranges;

struct system_environment
{
public:
    string crawl_name;
    string crawl_rc;
    string crawl_dir;

    vector<string> rcdirs;        // Directories to search for includes.

    string morgue_dir;
    string macro_dir;
    string crawl_base;             // Directory from argv[0], may be used to
                                   // locate datafiles.
    string crawl_exe;              // File from argv[0].
    string home;

#ifdef DGL_SIMPLE_MESSAGING
    string messagefile;            // File containing messages from other users.
    bool have_messages;            // There are messages waiting to be read.
    unsigned  message_check_tick;
#endif

    string scorefile;
    vector<string> cmd_args;

    int map_gen_iters;
    unique_ptr<depth_ranges> map_gen_range;

    vector<string> extra_opts_first;
    vector<string> extra_opts_last;

public:
    void add_rcdir(const string &dir);
};

extern system_environment SysEnv;

bool parse_args(int argc, char **argv, bool rc_only);

struct newgame_def;
void write_newgame_options_file(const newgame_def& prefs);

void save_player_name();

string channel_to_str(int ch);

int str_to_channel(const string &);
weapon_type str_to_weapon(const string &str);

class StringLineInput : public LineInput
{
public:
    StringLineInput(const string &s) : str(s), pos(0) { }

    bool eof() override
    {
        return pos >= str.length();
    }

    string get_line() override
    {
        if (eof())
            return "";
        string::size_type newl = str.find("\n", pos);
        if (newl == string::npos)
            newl = str.length();
        string line = str.substr(pos, newl - pos);
        pos = newl + 1;
        return line;
    }
private:
    const string &str;
    string::size_type pos;
};
