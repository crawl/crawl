/**
 * @file
 * @brief Functions for inventory related commands.
**/

#include "AppHdr.h"

#include "command-palette-menu.h"
#include "message.h"
#include "output.h"
#include "macro.h"

std::vector<std::pair<std::string_view, command_type>> const CommandPalette::commands = {
    {"Autoexplore", CMD_EXPLORE},
    {"Autofight without movement", CMD_AUTOFIGHT_NOMOVE},
    {"Autofight", CMD_AUTOFIGHT},
    {"Cast spell", CMD_CAST_SPELL},
    {"Display Inventory", CMD_DISPLAY_INVENTORY},
    {"Display Known Items", CMD_DISPLAY_KNOWN_OBJECTS},
    {"Display Map", CMD_DISPLAY_MAP},
    {"Display Mutations", CMD_DISPLAY_MUTATIONS},
    {"Display Religion", CMD_DISPLAY_RELIGION},
    {"Display Runes", CMD_DISPLAY_RUNES},
    {"Display Skills", CMD_DISPLAY_SKILLS},
    {"Edit player's tile", CMD_EDIT_PLAYER_TILE},
    {"Game Menu", CMD_GAME_MENU},
    {"Go downstairs", CMD_GO_DOWNSTAIRS},
    {"Go upstairs", CMD_GO_UPSTAIRS},
    {"List armour", CMD_LIST_ARMOUR},
    {"List gold", CMD_LIST_GOLD},
    {"List jewellery", CMD_LIST_JEWELLERY},
    {"Memorise Spell", CMD_MEMORISE_SPELL},
    {"Quit", CMD_QUIT},
    {"Rest", CMD_REST},
    {"Save Game and quit", CMD_SAVE_GAME},
    {"Toggle Sound", CMD_TOGGLE_SOUND},
    {"Toggle auto pickup", CMD_TOGGLE_AUTOPICKUP},
    {"Zoom in", CMD_ZOOM_IN},
    {"Zoom out", CMD_ZOOM_OUT},
};

CommandPaletteEntry::CommandPaletteEntry(std::string_view txt, command_type cmd)
    : MenuEntry(std::string(txt)), cmd(cmd), command_description(text) {}

bool CommandPaletteEntry::is_entry_hoverable() const
{
    return true;
}

CommandPalette::CommandPalette()
    : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
            | MF_ARROWS_SELECT | MF_WRAP | MF_INIT_HOVER | MF_USE_TWO_COLUMNS)
{
    set_tag("cmd_palette");
    menu_action = ACT_EXECUTE;

    for (auto const& it: CommandPalette::commands)
    {
        add_command(new CommandPaletteEntry{it.first, it.second});
    }

    title = new MenuEntry("Command Palette:", MEL_TITLE);
    title->colour = LIGHTBLUE;

    title2 = new MenuEntry("", MEL_TITLE);
    title2->colour = GREEN;

    CommandPalette::update_title();
}

void CommandPalette::add_command(CommandPaletteEntry *entry)
{
    entry->on_click = [this](MenuEntry &e)
    {
        on_command_click(dynamic_cast<CommandPaletteEntry &>(e));
    };

    all_entries.push_back(entry);
    add_entry(entry);
}

void CommandPalette::select_item_index(int idx, int qty)
{
    on_command_click(dynamic_cast<CommandPaletteEntry &>(*items[idx]));
    Menu::select_item_index(idx, qty);
}

void CommandPalette::update_title()
{
    auto fs = calc_title();

    if (title)
    {
        fs.textcolour(title->colour);
        fs += title->get_text() + " ";
    }

    if (title2)
    {
        fs.textcolour(title2->colour);
        fs += title2->get_text();
    }

    m_ui.title->set_text(fs);
}

bool CommandPalette::process_key(int keyin)
{
    char c = static_cast<char>(keyin);

    if (c == '\b')
    {
        remove_char();
        return true;
    }

    if (std::isalpha(c) || c == ' ')
    {
        add_char(c);
        return true;
    }

    return Menu::process_key(keyin);
}

void CommandPalette::update_items(std::string const &pattern)
{
    items.clear();
    std::vector<MenuEntry*> matched_entries;

    auto const& source = matching_entries.empty() ? all_entries : matching_entries;

    for (auto* entry : source)
    {
        auto * const as_command_entry = dynamic_cast<CommandPaletteEntry*>(entry);

        if (!as_command_entry)
            continue;

        size_t pos = as_command_entry->command_description.find(pattern);

        if (pos == std::string::npos)
            continue;

        entry->text = format_matching_string(as_command_entry->command_description, pattern, pos);
        matched_entries.push_back(entry);
        add_entry(entry);
    }

    matching_entries = std::move(matched_entries);
    entries_stack.push(matching_entries);

    update_menu(true);
    redraw_screen(true);
}

void CommandPalette::undo_update_items()
{
    if (entries_stack.empty())
        return;

    items.clear();
    entries_stack.pop();

    std::vector<MenuEntry*>* source = nullptr;

    if (entries_stack.empty())
    {
        matching_entries.clear();
        source = &all_entries;
    }
    else
    {
        matching_entries = entries_stack.top();
        source = &matching_entries;
    }

    for (auto const& entry : *source)
    {
        auto* as_command_entry = dynamic_cast<CommandPaletteEntry *>(entry);

        if (!as_command_entry)
            continue;

        entry->text =format_matching_string(as_command_entry->command_description,
            title2->text);
        add_entry(entry);
    }

    update_menu(true);
}

void CommandPalette::on_command_click(CommandPaletteEntry &entry)
{
    selectedCommand = &entry;
    close();
}

void CommandPalette::remove_char()
{
    if (title2 && !title2->text.empty())
    {
        title2->text.resize(title2->text.length() - 1);
        update_title();
        undo_update_items();
    }
}

void CommandPalette::add_char(char c)
{
    if (!title2)
        return;

    title2->text += c;
    update_title();
    update_items(title2->text);
}

std::string CommandPalette::format_matching_string(std::string const &str, std::string const &pattern,
    size_t patternPos)
{
    return formatted_string::parse_string(
                  str.substr(0, patternPos) + "<red>" + pattern + "</red>" +
                  str.substr(patternPos + pattern.size()),
                  MENU_ITEM_STOCK_COLOUR).to_colour_string();
}

std::string CommandPalette::format_matching_string(std::string const &str, std::string const &pattern)
{
    auto pos = str.find(pattern);

    return pos == std::string::npos ? "" : format_matching_string(str, pattern, pos);
}

command_type display_command_palette()
{
    CommandPalette menu;

    menu.show(true);

    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }

    if (menu.selectedCommand)
    {
        mpr("Selected command: " + command_to_name(menu.selectedCommand->cmd));
        return menu.selectedCommand->cmd;
    }

    return CMD_NO_CMD;
}
