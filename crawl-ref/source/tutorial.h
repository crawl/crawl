/*
 *  File:       tutorial.h
 *  Summary:    Stuff needed for tutorial
 *  Written by: JPEG
 *
 *  Created on 2007-01-11.
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

// any change in this list warrants an increase in the
// version number in tutorial.cc
enum tutorial_event
{
    TUT_SEEN_FIRST_OBJECT,  // 0
    // seen certain items
    TUT_SEEN_POTION,        
    TUT_SEEN_SCROLL,
    TUT_SEEN_WAND,
    TUT_SEEN_SPBOOK,
    TUT_SEEN_JEWELLERY,     // 5    
    TUT_SEEN_MISC,      
    TUT_SEEN_STAFF,
    TUT_SEEN_WEAPON,
    TUT_SEEN_MISSILES,
    TUT_SEEN_ARMOUR,        // 10
    TUT_SEEN_RANDART, 
    TUT_SEEN_FOOD,  
    TUT_SEEN_CARRION,
    // encountered dungeon features
    TUT_SEEN_STAIRS,        
    TUT_SEEN_TRAPS,         // 15
    TUT_SEEN_ALTAR,         
    TUT_SEEN_SHOP,          
    TUT_SEEN_DOOR,
    // other 'first events'
    TUT_SEEN_MONSTER,
    TUT_KILLED_MONSTER,     // 20
    TUT_NEW_LEVEL,       
    TUT_SKILL_RAISE,
    TUT_YOU_ENCHANTED,
    TUT_YOU_SICK,   
    TUT_YOU_POISON,         // 25
    TUT_YOU_CURSED,         
    TUT_YOU_HUNGRY, 
    TUT_YOU_STARVING,
    TUT_MULTI_PICKUP,
    TUT_HEAVY_LOAD,         // 30
    TUT_ROTTEN_FOOD,
    TUT_NEED_HEALING,       
    TUT_NEED_POISON_HEALING,
    TUT_RUN_AWAY,   
    TUT_MAKE_CHUNKS,        // 35
    TUT_POSTBERSERK, 
    TUT_SHIFT_RUN,
    TUT_MAP_VIEW,   
    TUT_DONE_EXPLORE,
    TUT_YOU_MUTATED,        // 40
    TUT_NEW_ABILITY,  
    TUT_WIELD_WEAPON,
    TUT_EVENTS_NUM          // 43
}; // for numbers higher than 45 change size of tutorial_events in externs.h

enum tutorial_types
{
    TUT_BERSERK_CHAR,
    TUT_MAGIC_CHAR,
    TUT_RANGER_CHAR,
    TUT_TYPES_NUM   // 3
};    

#endif
