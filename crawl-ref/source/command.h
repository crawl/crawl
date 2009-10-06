/*
 *  File:       command.h
 *  Summary:    Misc commands.
 *  Written by: Linley Henzell
 */


#ifndef COMMAND_H
#define COMMAND_H

#include "enum.h"

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void adjust();
void list_weapons();
void list_armour();
void list_jewellery();

/* ***********************************************************************
 * called from: clua
 * *********************************************************************** */
void swap_inv_slots(int slot1, int slot2, bool verbose);

void show_levelmap_help();
void show_targetting_help();
void show_interlevel_travel_branch_help();
void show_interlevel_travel_depth_help();
void show_stash_search_help();
void show_butchering_help();
void list_commands(int hotkey = 0, bool do_redraw_screen = false);
#ifdef WIZARD
int list_wizard_commands(bool do_redraw_screen = false);
#endif

// XXX: Actually defined in acr.cc; we may want to move this to command.cc.
void process_command(command_type cmd);

#endif
