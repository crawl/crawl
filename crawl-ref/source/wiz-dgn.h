/*
 *  File:       wiz-dgn.h
 *  Summary:    Dungeon related wizard functions.
 *  Written by: Linley Henzell and Jesse Jones
 */

#ifndef WIZDGN_H
#define WIZDGN_H

#include <string>

void wizard_create_portal();
void wizard_create_feature();
void wizard_list_branches();
void wizard_reveal_traps();
void wizard_map_level();
void wizard_list_items();
void wizard_place_stairs(bool down);
void wizard_level_travel(bool down);
void wizard_interlevel_travel();
void wizard_list_levels();
void debug_make_trap(void);
void debug_make_shop(void);
void debug_place_map();
void debug_test_explore();

#endif
