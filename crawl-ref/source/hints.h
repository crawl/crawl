/*
 *  File:       hints.h
 *  Summary:    Stuff needed for hints mode
 *  Written by: j-p-e-g
 *
 *  Created on 2007-01-11.
 */

#ifndef HINTS_H
#define HINTS_H

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

#include "externs.h"
#include "mon-info.h"

class formatted_string;
class writer;
class reader;

enum hints_types
{
    HINT_BERSERK_CHAR,
    HINT_MAGIC_CHAR,
    HINT_RANGER_CHAR,
    HINT_TYPES_NUM   // 3
};

void save_hints(writer& outf);
void load_hints(reader& inf);
void init_hints_options(void);

struct newgame_def;
void init_hints();
void pick_hints(newgame_def* choice);
void hints_load_game(void);
void print_hints_menu(unsigned int type);
void hints_zap_secret_doors(void);

formatted_string hints_starting_info2();
void hints_starting_screen(void);
void hints_new_turn();
void hints_death_screen(void);
void hints_finished(void);

void hints_dissection_reminder(bool healthy);
void hints_healing_reminder(void);

void taken_new_item(unsigned char item_type);
void hints_gained_new_skill(int skill);
void hints_monster_seen(const monsters& mon);
void hints_first_item(const item_def& item);
void learned_something_new(hints_event_type seen_what,
                           coord_def gc = coord_def());
formatted_string hints_abilities_info(void);
void print_hints_skills_info(void);
void print_hints_skills_description_info(void);

// Additional information for tutorial players.
void hints_describe_item(const item_def &item);
void hints_inscription_info(bool autoinscribe, std::string prompt);
bool hints_pos_interesting(int x, int y);
void hints_describe_pos(int x, int y);
bool hints_monster_interesting(const monsters *mons);
void hints_describe_monster(const monster_info& mi, bool has_stat_desc);

void hints_observe_cell(const coord_def& gc);

struct hints_state
{
    FixedVector<bool, 85> hints_events;
    bool hints_explored;
    bool hints_stashes;
    bool hints_travel;
    unsigned int hints_spell_counter;
    unsigned int hints_throw_counter;
    unsigned int hints_berserk_counter;
    unsigned int hints_melee_counter;
    unsigned int hints_last_healed;
    unsigned int hints_seen_invisible;

    bool hints_just_triggered;
    unsigned int hints_type;
    unsigned int hints_left;
};

extern hints_state Hints;

#endif
