#include "AppHdr.h"

#include "ng-input.h"

#include <cwctype>

#include "cio.h"
#include "end.h"
#include "files.h"
#include "format.h"
#include "libutil.h"
#include "options.h"
#include "stringutil.h"
#include "unicode.h"
#include "version.h"

extern string init_file_error; // defined in main.cc

// Eventually, this should be something more grand. {dlb}
void opening_screen()
{
    string msg =
    "<yellow>Hello, welcome to " CRAWL " " + string(Version::Long) + "!</yellow>\n"
    "<brown>(c) Copyright 1997-2002 Linley Henzell, "
    "2002-2014 Crawl DevTeam\n"
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
    textcolour(LIGHTGREY);
}

static void _show_name_prompt(int where)
{
    cgotoxy(1, where);
    textcolour(CYAN);

    cprintf("\nWhat is your name today? ");

    textcolour(LIGHTGREY);
}

bool is_good_name(const string& name, bool blankOK, bool verbose)
{
    // verification begins here {dlb}:
    if (name.empty())
    {
        if (blankOK)
            return true;

        if (verbose)
            cprintf("\nThat's a silly name!\n");
        return false;
    }

    return validate_player_name(name, verbose);
}

static bool _read_player_name(string &name)
{
    const int name_x = wherex(), name_y = wherey();
    char buf[kNameLen + 1]; // FIXME: make line_reader handle widths
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
            return true;
        }

        if (key_is_escape(ret))
            return false;

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

bool validate_player_name(const string &name, bool verbose)
{
#if defined(TARGET_OS_WINDOWS)
    // Quick check for CON -- blows up real good under DOS/Windows.
    if (strcasecmp(name.c_str(), "con") == 0
        || strcasecmp(name.c_str(), "nul") == 0
        || strcasecmp(name.c_str(), "prn") == 0
        || strnicmp(name.c_str(), "LPT", 3) == 0)
    {
        if (verbose)
            cprintf("\nSorry, that name gives your OS a headache.\n");
        return false;
    }
#endif

    if (strwidth(name) > kNameLen)
    {
        if (verbose)
            cprintf("\nThat name is too long.\n");
        return false;
    }

    ucs_t c;
    for (const char *str = name.c_str(); int l = utf8towc(&c, str); str += l)
    {
        // The technical reasons are gone, but enforcing some sanity doesn't
        // hurt.
        if (!iswalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
        {
            if (verbose)
            {
                cprintf("\n"
                        "Alpha-numerics, spaces, dashes, periods "
                        "and underscores only, please."
                        "\n");
            }
            return false;
        }
    }

    return true;
}
