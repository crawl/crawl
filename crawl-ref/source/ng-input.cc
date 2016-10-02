#include "AppHdr.h"

#include "ng-input.h"

#include <cwctype>

#include "cio.h"
#include "end.h"
#include "files.h"
#include "format.h"
#include "itemname.h" // make_name
#include "initfile.h"
#include "libutil.h"
#include "options.h"
#include "stringutil.h"
#include "unicode.h"
#include "version.h"

// Eventually, this should be something more grand. {dlb}
void opening_screen()
{
    string msg =
    "<yellow>Hello, welcome to " CRAWL " " + string(Version::Long) + "!</yellow>\n"
    "<brown>(c) Copyright 1997-2002 Linley Henzell, 2002-2016 Crawl DevTeam\n"
    "Read the instructions for legal details.</brown> ";


    FileLineInput f(Options.filename.c_str());

    if (!f.error())
    {
        msg += "<lightgrey>(Options read from ";
#ifdef DGAMELAUNCH
        // For dgl installs, show only the last segment of the .crawlrc
        // file name so that we don't leak details of the directory
        // structure to (untrusted) users.
        msg += Options.basefilename;
#else
        msg += Options.filename;
#endif
        msg += ".)</lightgrey>";
    }
    else
    {
        msg += "<lightred>(Options file ";
        if (!Options.filename.empty())
        {
            msg += make_stringf("\"%s\" is not readable",
                                Options.filename.c_str());
        }
        else
            msg += "not found";
        msg += "; using defaults.)</lightred>";
    }

    msg += "\n";

    formatted_string::parse_string(msg).display();
    textcolour(LIGHTGREY);
}

static void _show_name_prompt(int where)
{
    cgotoxy(1, where);
    textcolour(CYAN);

    cprintf("\nWhat is your name today? (Leave blank for a random name, or use Escape to go back.) ");

    textcolour(LIGHTGREY);
}

bool is_good_name(const string& name, bool blankOK, bool verbose)
{
    // verification begins here {dlb}:
    // Disallow names that would result in a save named just ".cs".
    if (strip_filename_unsafe_chars(name).empty())
    {
        if (blankOK && name.empty())
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
    char buf[MAX_NAME_LENGTH + 1]; // FIXME: make line_reader handle widths
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

/**
 * Attempt to generate a random name for a character that doesn't collide with
 * an existing save name.
 *
 * @return  A random name, or the empty string if no good name could be
 *          generated after several tries.
 */
static string _random_name()
{
    for (int i = 0; i < 100; ++i)
    {
        const string name = make_name();
        const string filename = get_save_filename(name);
        if (!save_exists(filename))
            return name;
    }

    return "";
}

// Reads a valid name from the player, writing it to ng.name.
void enter_player_name(newgame_def& ng)
{
    int prompt_start = wherey();

    do
    {
        // Prompt for a new name if current one unsatisfactory {dlb}:
        _show_name_prompt(prompt_start);

        // If the player wants out, we bail out.
        if (!_read_player_name(ng.name))
            end(0);
        trim_string(ng.name);

        if (ng.name.empty())
            ng.name = _random_name();
    }
    while (!is_good_name(ng.name, false, true));
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

    if (strwidth(name) > MAX_NAME_LENGTH)
    {
        if (verbose)
            cprintf("\nThat name is too long.\n");
        return false;
    }

    char32_t c;
    for (const char *str = name.c_str(); int l = utf8towc(&c, str); str += l)
    {
        // The technical reasons are gone, but enforcing some sanity doesn't
        // hurt.
        if (!iswalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
        {
            if (verbose)
            {
                cprintf("\n"
                        "Alpha-numerics, spaces, hyphens, periods "
                        "and underscores only, please."
                        "\n");
            }
            return false;
        }
    }

    return true;
}
