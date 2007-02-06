/*
 *  File:       tutorial.h
 *  Summary:    Stuff needed for tutorial
 *  Written by: JPEG
 *
 *	Created on 2007-01-11.
 */

#ifndef TUTORIAL_H
#define TUTORIAL_H

// for formatted_string
#include "menu.h"

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

void save_tutorial( FILE* fp );
void load_tutorial( FILE* fp );
void init_tutorial_options(void);
bool pick_tutorial(void);
void print_tutorial_menu(unsigned int type);
unsigned int get_tutorial_species(unsigned int type);
unsigned int get_tutorial_job(unsigned int type); 
//formatted_string tut_starting_info(unsigned int width);
formatted_string tut_starting_info2();
void tut_starting_screen();
void tutorial_death_screen(void);
void tutorial_finished(void);
void tutorial_prayer_reminder(void);
void tutorial_healing_reminder(void);
void taken_new_item(unsigned char item_type);
void tutorial_first_monster(const monsters mon);
void tutorial_first_item(const item_def item);
void learned_something_new(unsigned int seen_what, int x=0, int y=0);
//void learned_something_new(unsigned int seen_what/*, formatted_string st = formatted_string::parse_string("") */);
//void tutorial_output_commands(const std::string &str, unsigned int colour);

enum tutorial_event
{
    TUT_SEEN_FIRST_OBJECT,	// 0
    /* seen certain items */
    TUT_SEEN_POTION, 		
    TUT_SEEN_SCROLL,
    TUT_SEEN_WAND,
    TUT_SEEN_SPBOOK,
    TUT_SEEN_JEWELLERY,		// 5	
    TUT_SEEN_MISC,		
    TUT_SEEN_WEAPON,
    TUT_SEEN_MISSILES,
    TUT_SEEN_ARMOUR,
    TUT_SEEN_RANDART,		// 10
    TUT_SEEN_FOOD,	
    TUT_SEEN_CARRION,
    TUT_SEEN_STAIRS,		
    TUT_SEEN_TRAPS,
    TUT_SEEN_ALTAR,			// 15
    TUT_SEEN_SHOP,			// not tested so far
    TUT_SEEN_DOOR,
    /* other 'first events */
    TUT_SEEN_MONSTER,
    TUT_KILLED_MONSTER,		
    TUT_NEW_LEVEL,			// 20
    TUT_SKILL_RAISE,
    TUT_YOU_ENCHANTED,
    TUT_YOU_SICK,	
    TUT_YOU_POISON,		
    TUT_YOU_CURSED,			// 25
    TUT_YOU_HUNGRY,	
    TUT_YOU_STARVING,
    TUT_MULTI_PICKUP,
    TUT_HEAVY_LOAD,	
    TUT_ROTTEN_FOOD, 		// 30
    TUT_NEED_HEALING,		
    TUT_NEED_POISON_HEALING,
    TUT_RUN_AWAY,	
    TUT_MAKE_CHUNKS,
    TUT_POSTBERSERK,		// 35
    TUT_SHIFT_RUN,
    TUT_MAP_VIEW,	
//    TUT_AUTOEXPLORE,
    TUT_DONE_EXPLORE,
    TUT_YOU_MUTATED,		// 39
    TUT_NEW_ABILITY,
    TUT_WIELD_WEAPON,
    TUT_EVENTS_NUM 			// 42
}; // for numbers higher than 45 change size of tutorial_events in externs.h

enum tutorial_types
{
//    TUT_MELEE_CHAR,	// 0
    TUT_BERSERK_CHAR,
    TUT_MAGIC_CHAR,
    TUT_RANGER_CHAR,
    TUT_TYPES_NUM	// 4
};    

#endif
