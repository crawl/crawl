/*
 *  File:       command.h
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

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
void adjust(void);
void list_weapons(void);
void list_armour(void);
void list_jewellery(void);

/* ***********************************************************************
 * called from: clua
 * *********************************************************************** */
void swap_inv_slots(int slot1, int slot2, bool verbose);

void show_levelmap_help();
void show_targeting_help();
void show_interlevel_travel_branch_help();
void show_interlevel_travel_depth_help();
void show_stash_search_help();
void list_commands(bool wizard, int hotkey = 0,
                   bool do_redraw_screen = false);

// XXX: Actually defined in acr.cc; we may want to move this to command.cc.
void process_command(command_type cmd);

#endif
