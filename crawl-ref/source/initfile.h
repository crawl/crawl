/*
 *  File:       initfile.h
 *  Summary:    Simple reading of init file.
 *  Written by: David Loewenstern
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     6/9/99        DML             Created
 */


#ifndef INITFILE_H
#define INITFILE_H

#include <string>
#include <cstdio>

std::string & trim_string( std::string &str );

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void read_init_file(bool runscript = false);

void read_options(FILE *f, bool runscript = false);

void read_options(const std::string &s, bool runscript = false);

void parse_option_line(const std::string &line, bool runscript = false);

void apply_ascii_display(bool ascii);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void get_system_environment(void);


// last updated 16feb2001 {gdl}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool parse_args(int argc, char **argv, bool rc_only);

void write_newgame_options_file(void);

void save_player_name(void);

std::string channel_to_str(int ch);

int str_to_channel(const std::string &);

class InitLineInput {
public:
    virtual ~InitLineInput() { }
    virtual bool eof() = 0;
    virtual std::string getline() = 0;
};

class FileLineInput : public InitLineInput {
public:
    FileLineInput(FILE *f) : file(f) { }

    bool eof() {
        return !file || feof(file);
    }

    std::string getline() {
        char s[256] = "";
        if (!eof())
            fgets(s, sizeof s, file);
        return (s);
    }
private:
    FILE *file;
};

class StringLineInput : public InitLineInput {
public:
    StringLineInput(const std::string &s) : str(s), pos(0) { }

    bool eof() {
        return pos >= str.length();
    }

    std::string getline() {
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
