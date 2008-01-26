/*
 *  File:       command.cc
 *  Summary:    Misc commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     -/--/--        LRH             Created
 */


#ifndef COMMAND_H
#define COMMAND_H

#include "enum.h"

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void quit_game(void);

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void version(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void adjust(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void list_weapons(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void list_armour(void);


// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void list_jewellery(void);

void swap_inv_slots(int slot1, int slot2, bool verbose);

void show_levelmap_help();
void show_targeting_help();
void show_interlevel_travel_branch_help();
void show_interlevel_travel_depth_help();
void show_stash_search_help();
void list_commands(bool wizard, int hotkey = 0,
                   bool do_redraw_screen = false);
void list_tutorial_help(void);

// Actually defined in acr.cc; we may want to move this to command.cc
void process_command(command_type cmd);

#endif
