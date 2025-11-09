/**
 * @file
 * @brief Pause menu implementation
**/

#include "AppHdr.h"

#include "pause-menu.h"

#include "branch.h"
#include "cio.h"
#include "command.h"
#include "chardump.h"
#include "describe.h"
#include "end.h"
#include "files.h"
#include "game-exit-type.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "ouch.h"
#include "player.h"
#include "prompt.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "tilesdl.h"

PauseMenu::PauseMenu()
    : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
            | MF_ARROWS_SELECT | MF_WRAP | MF_INIT_HOVER),
      selected_cmd(CMD_NO_CMD)
{
    set_tag("pause");
    set_title("Game Paused");
    
    // Set up the selection handler for mouse clicks and enter key
    on_single_selection = [this](const MenuEntry& item)
    {
        const PauseMenuEntry *entry = dynamic_cast<const PauseMenuEntry *>(&item);
        if (entry)
        {
            selected_cmd = entry->cmd;
            return false; // Exit menu
        }
        return true; // Continue showing menu
    };
    
    fill_entries();
}

bool PauseMenu::skip_process_command(int keyin)
{
    // Handle F2 key to toggle menu (same as pause key)
    if (keyin == CK_F2)
    {
        selected_cmd = CMD_NO_CMD; // Resume game
        return false; // Exit menu
    }
    // Handle escape key to resume game
    if (keyin == ESCAPE)
    {
        selected_cmd = CMD_NO_CMD; // Resume game
        return false; // Exit menu
    }
    
    // Handle menu item hotkeys
    switch (keyin)
    {
    case 'r': case 'R':
        selected_cmd = CMD_NO_CMD; // Resume
        return false;
    case 'c': case 'C':
        selected_cmd = CMD_DISPLAY_CHARACTER_STATUS;
        return false;
    case 'm': case 'M':
        selected_cmd = CMD_SAVE_GAME_NOW;
        return false;
    case 'q': case 'Q':
        selected_cmd = CMD_QUIT;
        return false;
    }
    
    return Menu::skip_process_command(keyin);
}

void PauseMenu::fill_entries()
{
    clear();
    add_entry(new PauseMenuEntry("", 0, CMD_NO_CMD, false)); // Subtitle spacer
    
    add_entry(new PauseMenuEntry("Resume Game", 'r', CMD_NO_CMD, true));
    add_entry(new PauseMenuEntry("Character Stats", 'c', CMD_DISPLAY_CHARACTER_STATUS, false));
    add_entry(new PauseMenuEntry("Return to Main Menu", 'm', CMD_SAVE_GAME_NOW, false));
    add_entry(new PauseMenuEntry("", 0, CMD_NO_CMD, false)); // Subtitle spacer
    add_entry(new PauseMenuEntry("Quit Game", 'q', CMD_QUIT, false));
}

vector<MenuEntry *> PauseMenu::show(bool reuse_selections)
{
    vector<MenuEntry *> result = Menu::show(reuse_selections);
    
    if (!result.empty() && result[0])
    {
        PauseMenuEntry* entry = static_cast<PauseMenuEntry*>(result[0]);
        selected_cmd = entry->cmd;
    }
    else
    {
        selected_cmd = CMD_NO_CMD; // Resume game if no selection
    }
    
    return result;
}

void PauseMenu::show_character_stats()
{
    clrscr();
    
    // Display character information similar to the character dump
    string stats;
    
    // Basic character info
    stats += make_stringf("Name: %s\n", you.your_name.c_str());
    stats += make_stringf("Species: %s\n", species::name(you.species).c_str());
    stats += make_stringf("Job: %s\n", get_job_name(you.char_class));
    stats += make_stringf("Level: %d\n", you.experience_level);
    
    // Health and magic
    stats += make_stringf("HP: %d/%d\n", you.hp, you.hp_max);
    stats += make_stringf("MP: %d/%d\n", you.magic_points, you.max_magic_points);
    
    // Defenses
    stats += make_stringf("AC: %d\n", you.armour_class());
    stats += make_stringf("EV: %d\n", you.evasion());
    stats += make_stringf("SH: %d\n", you.shield_bonus());
    
    // Attributes
    stats += make_stringf("Str: %d\n", you.strength());
    stats += make_stringf("Int: %d\n", you.intel());
    stats += make_stringf("Dex: %d\n", you.dex());
    
    // Current location
    if (you.where_are_you != BRANCH_DUNGEON)
        stats += make_stringf("Branch: %s\n", branches[you.where_are_you].longname);
    stats += make_stringf("Depth: %d\n", you.depth);
    
    // Religion
    if (!you_worship(GOD_NO_GOD))
    {
        stats += make_stringf("God: %s\n", god_name(you.religion).c_str());
        stats += make_stringf("Piety: %d\n", you.piety());
    }
    
    formatted_string fs;
    fs.cprintf("%s", stats.c_str());
    
    clrscr();
    const int lines = count(stats.begin(), stats.end(), '\n') + 1;
    const int start_y = (get_number_of_lines() - lines) / 2;
    
    cgotoxy(1, start_y);
    fs.display();
    
    textcolour(LIGHTGREY);
    cprintf("\nPress any key to return to pause menu...");
    
    getchm();
}
