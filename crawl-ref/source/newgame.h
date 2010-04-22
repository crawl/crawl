/*
 *  File:       newgame.h
 *  Summary:    Functions used when starting a new game.
 *  Written by: Linley Henzell
 */


#ifndef NEWGAME_H
#define NEWGAME_H


#include "enum.h"
#include "itemprop-enum.h"

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

    weapon_type weapon;
    startup_book_type book;
    god_type religion;
    startup_wand_type wand;

    newgame_def();
};

undead_state_type get_undead_state(const species_type sp);

bool choose_game(newgame_def *ng);

void make_rod(item_def &item, stave_type rod_type, int ncharges);
int claws_level(species_type sp);
int start_to_wand(int wandtype, bool& is_rod);
int start_to_book(int firstbook, int booktype);

#endif
