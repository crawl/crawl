/**
 * @file
 * @brief Pause menu for pausing the game and showing options
**/

#pragma once

#include "menu.h"

class PauseMenu : public Menu
{
public:
    class PauseMenuEntry : public MenuEntry
    {
    public:
        PauseMenuEntry(const string &txt, int hotkey, command_type _cmd, bool preselect = true)
            : MenuEntry(txt, MEL_ITEM, 1, hotkey), cmd(_cmd)
        {
            data = &cmd;
            if (preselect)
                selected_qty = 1;
        }

        string get_text() const override
        {
            return text;
        }

        command_type cmd;
    };

    command_type selected_cmd;
    PauseMenu();
    
    bool skip_process_command(int keyin) override;
    void fill_entries();
    vector<MenuEntry *> show(bool reuse_selections = false) override;
    void show_character_stats();
    void show_keybinds();
};
