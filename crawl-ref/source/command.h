/**
 * @file
 * @brief Misc commands.
**/

#ifndef COMMAND_H
#define COMMAND_H

#include "enum.h"

void list_armour();
void list_jewellery();

void toggle_viewport_monster_hp();
void toggle_viewport_weapons();
void show_levelmap_help();
void show_pickup_menu_help();
void show_known_menu_help();
void show_targeting_help();
void show_interlevel_travel_branch_help();
void show_interlevel_travel_depth_help();
void show_stash_search_help();
void show_butchering_help();
void show_skill_menu_help();
void list_commands(int hotkey = 0, bool do_redraw_screen = false,
                   string highlight_string = "");
#ifdef WIZARD
int list_wizard_commands(bool do_redraw_screen = false);
#endif

// XXX: Actually defined in main.cc; we may want to move this to command.cc.
void process_command(command_type cmd);

#endif
