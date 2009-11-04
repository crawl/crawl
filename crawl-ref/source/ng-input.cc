#include "AppHdr.h"

#include "ng-input.h"
#include "newgame.h"

#include "cio.h"
#include "files.h"
#include "menu.h"
#include "options.h"
#include "stuff.h"

extern std::string init_file_error; // defined in acr.cc

// Eventually, this should be something more grand. {dlb}
void opening_screen(void)
{
#ifdef USE_TILE
    // More grand... Like this? ;)
    if (Options.tile_title_screen)
        tiles.draw_title();
#endif

    std::string msg =
    "<yellow>Hello, welcome to " CRAWL " " + Version::Long() + "!</yellow>" EOL
    "<brown>(c) Copyright 1997-2002 Linley Henzell, "
    "2002-2009 Crawl DevTeam" EOL
    "Please consult crawl_manual.txt for instructions and legal details."
    "</brown>" EOL;

    const bool init_found = init_file_error.empty();

    if (!init_found)
        msg += "<lightred>No init file ";
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
        msg += ", using defaults.";
    }

    msg += EOL;

    formatted_string::parse_string(msg).display();
    textcolor( LIGHTGREY );
}

static void _show_name_prompt(int where, bool blankOK,
                              const std::vector<player_save_info> &existing_chars,
                              slider_menu &menu)
{
    cgotoxy(1, where);
    textcolor( CYAN );
    if (blankOK)
    {
        if (Options.prev_name.length() && Options.remember_name)
        {
            cprintf(EOL
                    "Press <Enter> for \"%s\", or . to be prompted later."
                    EOL,
                    Options.prev_name.c_str());
        }
        else
        {
            cprintf(EOL
                    "Press <Enter> to answer this after species and "
                    "job are chosen." EOL);
        }
    }

    cprintf(EOL "What is your name today? ");

    if (!existing_chars.empty())
    {
        const int name_x = wherex(), name_y = wherey();
        menu.set_limits(name_y + 3, get_number_of_lines());
        menu.display();
        cgotoxy(name_x, name_y);
    }

    textcolor( LIGHTGREY );
}

static void _preprocess_character_name(std::string &name, bool blankOK)
{
    if (name.empty() && blankOK && Options.prev_name.length()
        && Options.remember_name)
    {
        name = Options.prev_name;
    }

    // '.', '?' and '*' are blanked.
    if (name.length() == 1
        && (name[0] == '.' || name[0] == '*' || name[0] == '?'))
    {
        name = "";
    }
}

static bool _is_good_name(std::string &name, bool blankOK, bool verbose)
{
    _preprocess_character_name(name, blankOK);

    // verification begins here {dlb}:
    if (name.empty())
    {
        if (blankOK)
            return (true);

        if (verbose)
            cprintf(EOL "That's a silly name!" EOL);
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
            cprintf(EOL "That's a silly name!" EOL);
        return (false);
    }
#endif
    return (validate_player_name(name, verbose));
}

static int newname_keyfilter(int &ch)
{
    if (ch == CK_DOWN || ch == CK_PGDN || ch == '\t')
        return -1;

    return 1;
}

static bool _read_player_name(std::string &name,
                              const std::vector<player_save_info> &existing,
                              slider_menu &menu)
{
    const int name_x = wherex(), name_y = wherey();
    int (*keyfilter)(int &) = newname_keyfilter;
    if (existing.empty())
        keyfilter = NULL;
    char buf[kNameLen];
    // XXX: Prompt displays garbage otherwise, but don't really know why.
    //      Other places don't do this. --rob
    buf[0] = '\0';
    line_reader reader(buf, sizeof(buf));

    reader.set_keyproc(keyfilter);

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

        if (ret == CK_ESCAPE)
            return (false);

        if (ret != CK_ESCAPE && existing.size())
        {
            menu.set_search(name);
            menu.show();
            const MenuEntry *sel = menu.selected_entry();
            if (sel)
            {
                const player_save_info &p =
                *static_cast<player_save_info*>( sel->data );
                name = p.name;
                return (true);
            }
        }

        // Go back and prompt the user.
    }
}

// Reads a valid name from the player, writing it to ng.name.
void enter_player_name(newgame_def &ng, bool blankOK)
{
    int prompt_start = wherey();
    bool ask_name = true;
    std::vector<player_save_info> existing_chars;
    slider_menu char_menu(MF_SINGLESELECT | MF_NOWRAP, false);

    if (!ng.name.empty())
        ask_name = false;

    if (blankOK && (ask_name || !_is_good_name(ng.name, false, false)))
    {
        existing_chars = find_saved_characters();
        if (existing_chars.empty())
        {
            cgotoxy(1,12);
            formatted_string::parse_string(
                                           "  If you've never been here before, you might want to try out" EOL
                                           "  the Dungeon Crawl tutorial. To do this, press "
                                           "<white>Ctrl-T</white> on the next" EOL
                                           "  screen.").display();
        }
        else
        {
            MenuEntry *title = new MenuEntry("Or choose an existing character:");
            title->colour = LIGHTCYAN;
            char_menu.set_title( title );
            for (unsigned int i = 0; i < existing_chars.size(); ++i)
            {
                std::string desc = " " + existing_chars[i].short_desc();
                if (static_cast<int>(desc.length()) >= get_number_of_cols())
                    desc = desc.substr(0, get_number_of_cols() - 1);

#ifdef USE_TILE
                MenuEntry *me = new PlayerMenuEntry(desc);
#else
                MenuEntry *me = new MenuEntry(desc);
#endif
                me->data = &existing_chars[i];
                char_menu.add_entry(me);
            }
        }
    }

    do
    {
        // Prompt for a new name if current one unsatisfactory {dlb}:
        if (ask_name)
        {
            _show_name_prompt(prompt_start, blankOK, existing_chars, char_menu);

            // If the player wants out, we bail out.
            if (!_read_player_name(ng.name, existing_chars, char_menu))
                end(0);
            trim_string(ng.name);
        }
    }
    while (ask_name = !_is_good_name(ng.name, blankOK, true));
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
            cprintf(EOL "Sorry, that name gives your OS a headache." EOL);
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
        if (!isalnum(c) && c != '-' && c != '.' && c != '_' && c != ' ')
        {
            if (verbose)
            {
                cprintf( EOL
                        "Alpha-numerics, spaces, dashes, periods "
                        "and underscores only, please."
                        EOL );
            }
            return (false);
        }
    }

    return (true);
}
