/*
 *  File:       tutorial.h
 *  Summary:    Stuff needed for tutorial
 *  Written by: JPEG
 *
 *  Created on 2007-01-11.
 */

#ifndef TUTORIAL_H
#define TUTORIAL_H

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include "AppHdr.h"
#include "externs.h"

class formatted_string;

void save_tutorial( FILE* fp );
void load_tutorial( FILE* fp );
void init_tutorial_options(void);

bool pick_tutorial(void);
void print_tutorial_menu(unsigned int type);

formatted_string tut_starting_info2();
void tut_starting_screen();
void tutorial_death_screen(void);
void tutorial_finished(void);

void tutorial_dissection_reminder(bool healthy);
void tutorial_healing_reminder(void);

void taken_new_item(unsigned char item_type);
void tutorial_first_monster(const monsters& mon);
void tutorial_first_item(const item_def& item);
void learned_something_new(tutorial_event_type seen_what, int x=0, int y=0);
formatted_string tut_abilities_info();

// additional information for tutorial players
void tutorial_describe_item(const item_def &item);
bool tutorial_feat_interesting(dungeon_feature_type feat);
void tutorial_describe_feature(dungeon_feature_type feat);
bool tutorial_monster_interesting(const monsters *mons);
void tutorial_describe_monster(const monsters *mons);

#endif
