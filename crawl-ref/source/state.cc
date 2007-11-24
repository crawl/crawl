/*
 *  File:       state.cc
 *  Summary:    Game state functions.
 *  Written by: Matthew Cline
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-11-20T12:19:31.082781Z $
 *
 *  Change History (most recent first):
 *
 *   <1>    09/18/07      MPC    Created
 */

#include "AppHdr.h"
#include "externs.h"

#include "state.h"
#include "menu.h" // For print_formatted_paragraph()

void game_state::add_startup_error(const std::string &err)
{
    startup_errors.push_back(err);
}

void game_state::show_startup_errors()
{
    formatted_scroller error_menu;
    error_menu.set_flags(MF_NOSELECT | MF_ALWAYS_SHOW_MORE | MF_NOWRAP
                         | MF_EASY_EXIT);
    error_menu.set_more(
        formatted_string::parse_string(
                           "<cyan>[ + : Page down.   - : Page up."
                           "                    Esc or Enter to continue.]"));
    error_menu.set_title(
        new MenuEntry("Warning: Crawl encountered errors during startup:",
                      MEL_TITLE));
    for (int i = 0, size = startup_errors.size(); i < size; ++i)
        error_menu.add_entry(new MenuEntry(startup_errors[i]));
    error_menu.show();
}
