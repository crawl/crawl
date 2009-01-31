/*
 *  File:       tutorial.h
 *  Summary:    Stuff needed for tutorial
 *  Written by: j-p-e-g
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
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
class writer;
class reader;

void save_tutorial(writer& outf);
void load_tutorial(reader& inf);
void init_tutorial_options(void);

bool pick_tutorial(void);
void print_tutorial_menu(unsigned int type);
void tutorial_zap_secret_doors(void);

formatted_string tut_starting_info2();
void tut_starting_screen(void);
void tutorial_death_screen(void);
void tutorial_finished(void);

void tutorial_dissection_reminder(bool healthy);
void tutorial_healing_reminder(void);

void taken_new_item(unsigned char item_type);
void tut_gained_new_skill(int skill);
void tutorial_first_monster(const monsters& mon);
void tutorial_first_item(const item_def& item);
void learned_something_new(tutorial_event_type seen_what,
                           coord_def gc = coord_def());
formatted_string tut_abilities_info(void);
void print_tut_skills_info(void);
void print_tut_skills_description_info(void);

void tutorial_observe_cell(const coord_def& gc);

// Additional information for tutorial players.
void tutorial_describe_item(const item_def &item);
void tutorial_inscription_info(bool autoinscribe, std::string prompt);
bool tutorial_pos_interesting(int x, int y);
void tutorial_describe_pos(int x, int y);
bool tutorial_feat_interesting(dungeon_feature_type feat);
void tutorial_describe_feature(dungeon_feature_type feat);
bool tutorial_monster_interesting(const monsters *mons);
void tutorial_describe_monster(const monsters *mons);


#endif
