/**
 * @file
 * @brief Functions for inventory related commands.
**/

#pragma once
#include "AppHdr.h"

#include "menu.h"
#include "command-type.h"

class CommandPaletteEntry : public MenuEntry
{
public:
    CommandPaletteEntry(std::string_view txt, command_type cmd = CMD_NO_CMD);
    command_type cmd;
};

class CommandPalette : public Menu
{
public:
    CommandPalette();

    void add_command(CommandPaletteEntry* entry);

    bool exit_on_click() const override;
    void select_item_index(int idx, int qty = MENU_SELECT_INVERT) override;
    void update_title() override;
    bool process_key(int keyin) override;
    CommandPaletteEntry* selectedCommand = nullptr;

private:
    void update_items(std::string const& pattern);
    void undo_update_items();

    void on_command_click(CommandPaletteEntry& entry);
    std::vector<MenuEntry*> all_entries;
    std::vector<MenuEntry*> matching_entries;

    void remove_char();
    void add_char(char c);

    std::stack<std::vector<MenuEntry*>> entries_stack;

public:
    static std::vector<std::pair<std::string_view, command_type>> const commands;
};

command_type display_command_palette();
