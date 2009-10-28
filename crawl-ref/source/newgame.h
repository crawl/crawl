/*
 *  File:       newgame.h
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */


#ifndef NEWGAME_H
#define NEWGAME_H


#include "enum.h"
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

class player;
struct newgame_def
{
    std::string name;

    species_type species;
    job_type job;

    // TODO: fill in

    void init(const player &p);
    void save(player &p);
};

undead_state_type get_undead_state(const species_type sp);

/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool new_game();

int give_first_conjuration_book();
bool choose_race(void);
bool choose_class(void);

/* ***********************************************************************
 * called from: debug newgame
 * *********************************************************************** */
void give_basic_mutations(species_type speci);

#endif
