/*
 *  File:       initfile.h
 *  Summary:    Simple reading of init file.
 *  Written by: David Loewenstern
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */


#ifndef INITFILE_H
#define INITFILE_H

#include <string>
#include <cstdio>

#include "enum.h"

enum drop_mode_type
{
    DM_SINGLE,
    DM_MULTI
};

god_type str_to_god(std::string god);
int str_to_colour(const std::string &str, int default_colour = -1,
                  bool accept_number = true);
const std::string colour_to_str(unsigned char colour);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
std::string read_init_file(bool runscript = false);

void read_startup_prefs();
void read_options(FILE *f, bool runscript = false);
void read_options(const std::string &s, bool runscript = false,
                  bool clear_aliases = false);

void parse_option_line(const std::string &line, bool runscript = false);

void apply_ascii_display(bool ascii);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void get_system_environment(void);

struct system_environment
{
public:
    std::string crawl_name;
    std::string crawl_rc;
    std::string crawl_dir;

    std::vector<std::string> rcdirs;   // Directories to search for includes.

    std::string morgue_dir;
    std::string crawl_base;        // Directory from argv[0], may be used to
                                   // locate datafiles.
    std::string home;              // only used by MULTIUSER systems
    bool  board_with_nail;         // Easter Egg silliness

#ifdef DGL_SIMPLE_MESSAGING
    std::string messagefile;       // File containing messages from other users.
    bool have_messages;            // There are messages waiting to be read.
    unsigned  message_check_tick;
#endif

    std::string scorefile;
    std::vector<std::string> cmd_args;

    int map_gen_iters;

    std::string arena_teams;

public:
    void add_rcdir(const std::string &dir);
};

extern system_environment SysEnv;


// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool parse_args(int argc, char **argv, bool rc_only);

void write_newgame_options_file(void);

void save_player_name(void);

std::string channel_to_str(int ch);

int str_to_channel(const std::string &);

class InitLineInput
{
public:
    virtual ~InitLineInput() { }
    virtual bool eof() = 0;
    virtual std::string getline() = 0;
};

class FileLineInput : public InitLineInput
{
public:
    FileLineInput(FILE *f) : file(f) { }

    bool eof()
    {
        return !file || feof(file);
    }

    std::string getline()
    {
        char s[256] = "";
        if (!eof())
            fgets(s, sizeof s, file);
        return (s);
    }
private:
    FILE *file;
};

class StringLineInput : public InitLineInput
{
public:
    StringLineInput(const std::string &s) : str(s), pos(0) { }

    bool eof()
    {
        return pos >= str.length();
    }

    std::string getline()
    {
        if (eof())
            return "";
        std::string::size_type newl = str.find("\n", pos);
        if (newl == std::string::npos)
            newl = str.length();
        std::string line = str.substr(pos, newl - pos);
        pos = newl + 1;
        return line;
    }
private:
    const std::string &str;
    std::string::size_type pos;
};

#endif
