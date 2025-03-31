#include "AppHdr.h"
#include "keybindings.h"
#include "options.h"
#include "libutil.h"
#include "files.h"
#include <ncurses.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "message.h"

std::vector<Keybinding> movement_keys = {
    {"Move Up", "Up Arrow"},
    {"Move Down", "Down Arrow"},
    {"Move Left", "Left Arrow"},
    {"Move Right", "Rigth Arrow"},
};

// Structure to hold movement keybindings
struct KeybindingOption {
    std::string action;
    std::string key;
};

// Function to load keybindings from a config file
void load_keybindings()
{
    std::ifstream file(datafile_path("keybindings.txt").c_str());
    if (!file) return;

    std::string action, key;
    while (file >> action >> key)
    {
        for (auto &kb : movement_keys)
        {
            if (kb.action == action)
                kb.key = key;
        }
    }
    file.close();
}

// Function to save keybindings to a config file
void save_keybindings()
{
    std::ofstream file(datafile_path("keybindings.txt").c_str());
    for (const auto &kb : movement_keys)
    {
        file << kb.action << " " << kb.key << "\n";
    }
    file.close();
}

// Function to change a selected keybinding
void change_keybinding(int index)
{
    flushinp(); // Clear lingering input
    mprf("\nPress a new key for %s:", movement_keys[index].action.c_str());

    char new_key = getch();
    if (new_key == 27)  // ESC to cancel
    {
        mpr("Keybinding change canceled.");
        return;
    }

    // Check if key is already assigned
    for (const auto &binding : movement_keys)
    {
        if (binding.key == std::string(1, new_key))
        {
            mpr("Error: Key is already assigned. Choose a different key.");
            return;
        }
    }

    movement_keys[index].key = std::string(1, new_key);
    mprf("\nKeybinding updated! %s -> [%c]", movement_keys[index].action.c_str(), new_key);

    save_keybindings();
}