/*
 *  File:       newgame.h
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */


#ifndef NEWGAME_H
#define NEWGAME_H


#include "itemprop.h"

enum death_knight_type
{
    DK_NO_SELECTION,
    DK_NECROMANCY,
    DK_YREDELEMNUL,
    DK_RANDOM
};

enum startup_book_type
{
    SBT_NO_SELECTION = 0,
    SBT_FIRE,
    SBT_COLD,
    SBT_SUMM,
    SBT_RANDOM
};

enum startup_wand_type
{
    SWT_NO_SELECTION = 0,
    SWT_ENSLAVEMENT,
    SWT_CONFUSION,
    SWT_MAGIC_DARTS,
    SWT_FROST,
    SWT_FLAME,
    SWT_STRIKING, // actually a rod
    SWT_RANDOM
};

undead_state_type get_undead_state(const species_type sp);

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool new_game();
void initialise_item_descriptions();

int give_first_conjuration_book();
bool choose_race(void);
bool choose_class(void);

/* ***********************************************************************
 * called from: debug newgame
 * *********************************************************************** */
void give_basic_mutations(species_type speci);

bool validate_player_name(const char *name, bool verbose);

/* ***********************************************************************
 * called from: files.cc
 * *********************************************************************** */
int get_class_by_name(const char *name);

#endif
