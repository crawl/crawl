#include "AppHdr.h"

#include "ng-input.h"
#include "newgame.h"

#include "cio.h"
#include "files.h"
#include "menu.h"
#include "options.h"
#include "stuff.h"

extern std::string init_file_error; // defined in main.cc

// Eventually, this should be something more grand. {dlb}
void opening_screen(void)
{
    std::string msg =
    "<yellow>Hello, welcome to " CRAWL " " + Version::Long() + "!</yellow>\n"
    "<brown>(c) Copyright 1997-2002 Linley Henzell, "
    "2002-2010 Crawl DevTeam\n"
    "Read the instructions for legal details."
    "</brown> " ;

    const bool init_found = init_file_error.empty();

    if (!init_found)
        msg += "<lightred>(No init file ";
    else
        msg += "<lightgrey>(Read options from ";

    if (init_found)
    {
#ifdef DGAMELAUNCH
        // For dgl installs, show only the last segment of the .crawlrc
        // file name so that we don't leak details of the directory
        // structure to (untrusted) users.
        msg += get_base_filename(Options.filename);
#else
        msg += Options.filename;
#endif
        msg += ".)";
    }
    else
    {
        msg += init_file_error;
        msg += ", using defaults.)";
    }

    msg += "\n";

    formatted_string::parse_string(msg).display();
    textcolor( LIGHTGREY );
}

static void _show_name_prompt(int where)
{
    cgotoxy(1, where);
    textcolor( CYAN );

    cprintf("\nWhat is your name today? ");

    textcolor( LIGHTGREY );
}

bool is_good_name(const std::string& name, bool blankOK, bool verbose)
{
    // verification begins here {dlb}:
    if (name.empty())
    {
        if (blankOK)
            return (true);

        if (verbose)
            cprintf("\nThat's a silly name!\n");
        return (false);
    }

    // If MULTIUSER is defined, userid will be tacked onto the end
    // of each character's files, making bones a valid player name.
#ifndef MULTIUSER
    // This would cause big probs with ghosts.
    // What would? {dlb}
    // ... having the name "bones" of course! The problem comes from
    // the fact that bones files would have the exact same filename
    // as level files for a character named "bones".  -- bwr
    if (stricmp(name.c_str(), "bones") == 0)
    {
        if (verbose)
            cprintf("\nThat's a silly name!\n");
        return (false);
    }
#endif
    return (validate_player_name(name, verbose));
}

static bool _read_player_name(std::string &name)
{
    const int name_x = wherex(), name_y = wherey();
    char buf[kNameLen];
    // XXX: Prompt displays garbage otherwise, but don't really know why.
    //      Other places don't do this. --rob
    buf[0] = '\0';
    line_reader reader(buf, sizeof(buf));

    while (true)
    {
        cgotoxy(name_x, name_y);
        if (name_x <= 80)
            cprintf("%-*s", 80 - name_x + 1, "");

        cgotoxy(name_x, name_y);
        int ret = reader.read_line(false);
        if (!ret)
        {
            name = buf;
            return (true);
        }

        if (key_is_escape(ret))
            return (false);

        // Go back and prompt the user.
    }
}

// Reads a valid name from the player, writing it to ng.name.
void enter_player_name(newgame_def *ng)
{
    int prompt_start = wherey();

    do
    {
        // Prompt for a new name if current one unsatisfactory {dlb}:
        _show_name_prompt(prompt_start);

        // If the player wants out, we bail out.
        if (!_read_player_name(ng->name))
            end(0);
        trim_string(ng->name);
    }
    while (!is_good_name(ng->name, false, true));
}

bool validate_player_name(const std::string &name, bool verbose)
{
#if defined(TARGET_OS_DOS) || defined(TARGET_OS_WINDOWS)
    // Quick check for CON -- blows up real good under DOS/Windows.
    if (stricmp(name.c_str(), "con") == 0
        || stricmp(name.c_str(), "nul") == 0
        || stricmp(name.c_str(), "prn") == 0
        || strnicmp(name.c_str(), "LPT", 3) == 0)
    {
        if (verbose)
            cprintf("\nSorry, that name gives your OS a headache.\n");
        return (false);
    }
#endif

    for (unsigned int i = 0; i < name.length(); i++)
    {
        char c = name[i];
        // Note that this includes systems which may be using the
        // packaging system.  The packaging system is very simple
        // and doesn't take the time to escape every character that
        // might be a problem for some random shell or OS... so we
        // play it very conservative here.  -- bwr
        // Accented 8-bit letters are probably harmless here.  -- 1KB
        if (!isalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
        {
            if (verbose)
            {
                cprintf( "\n"
                        "Alpha-numerics, spaces, dashes, periods "
                        "and underscores only, please."
                        "\n" );
            }
            return (false);
        }
    }

    return (true);
}
