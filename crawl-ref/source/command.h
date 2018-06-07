/**
 * @file
 * @brief Misc commands, and functions for working with commands.
**/

#pragma once

#include <string>
#include <vector>

#include "command-type.h"
#include "enum.h"
#include "format.h"

void list_armour();
void list_jewellery();

void show_specific_help(const string& key);
void show_levelmap_help();
void show_targeting_help();
void show_interlevel_travel_branch_help();
void show_interlevel_travel_depth_help();
void show_stash_search_help();
void show_butchering_help();
void show_skill_menu_help();
void show_spell_library_help();

void list_commands(int hotkey = 0, string highlight_string = "");

int show_keyhelp_menu(const vector<formatted_string> &lines,
                      bool with_manual, bool easy_exit = false,
                      int hotkey = 0, string highlight_string = "");

// XXX: Actually defined in main.cc; we may want to move this to command.cc.
void process_command(command_type cmd);
